// Package exchange implements the exchange-specific order book wrapper.
package exchange

// This file implements an order book wrapper that extends the core orderbook
// with account tracking for self-trade prevention.

import (
	"sync"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
	hashsdk "github.com/finosorg-labs/hash-c/sdk"
)

// OrderBook wraps the core order book with exchange-specific functionality.
// It manages order lifecycle, account tracking, and integrates with self-trade
// prevention and trade reporting.
//
// Performance characteristics:
//   - AddOrder: O(log n) insertion into red-black tree
//   - RemoveOrder: O(log n) deletion from red-black tree
//   - UpdateOrder: O(log n) modification
//   - GetBestBid/Ask: O(1) cached access
//   - GetDepth(k): O(k) where k is number of levels requested
//
// Memory: Each order consumes approximately 100-150 bytes including
// overhead for tree nodes and account mapping.
//
// Concurrency: Thread-safe via mutex.
type OrderBook struct {
	mu             sync.RWMutex
	coreBook       *coreob.OrderBook
	symbol         string
	symbolID       uint32
	selfTradeCheck *SelfTradeCheck
	orderAccounts  *hashsdk.Int64Map[string] // orderID -> accountID mapping
}

// NewOrderBook creates a new exchange order book.
func NewOrderBook(symbol string, symbolID uint32, selfTradeCheck *SelfTradeCheck) *OrderBook {
	return &OrderBook{
		coreBook:       coreob.NewOrderBook(symbol, symbolID),
		symbol:         symbol,
		symbolID:       symbolID,
		selfTradeCheck: selfTradeCheck,
		orderAccounts:  hashsdk.NewInt64Map[string](),
	}
}

// AddOrder adds an order to the order book.
// The order is registered for account tracking and self-trade prevention.
func (ob *OrderBook) AddOrder(order *Order) error {
	if order == nil {
		return ErrNilOrder
	}

	ob.mu.Lock()
	defer ob.mu.Unlock()

	// Convert to core order
	coreOrder := &coreob.Order{
		OrderID:   order.OrderID,
		Price:     order.Price,
		Quantity:  order.Quantity,
		Side:      order.Side,
		Timestamp: order.Timestamp,
	}

	// Add to core order book
	if err := ob.coreBook.InsertOrder(coreOrder); err != nil {
		return err
	}

	// Track account mapping
	ob.orderAccounts.Set(order.OrderID, order.AccountID)

	// Register for self-trade prevention
	ob.selfTradeCheck.RegisterOrder(order.OrderID, order.AccountID, order.Side)

	return nil
}

// RemoveOrder removes an order from the order book.
func (ob *OrderBook) RemoveOrder(orderID int64) error {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	// Get account ID before removal
	accountID, exists := ob.orderAccounts.Get(orderID)
	if !exists {
		return ErrOrderNotFound
	}

	// Remove from core order book
	if err := ob.coreBook.DeleteOrder(orderID); err != nil {
		return err
	}

	// Unregister from self-trade prevention
	ob.selfTradeCheck.UnregisterOrder(orderID, accountID)

	// Remove account mapping
	ob.orderAccounts.Delete(orderID)

	return nil
}

// UpdateOrder updates an existing order in the order book.
// This modifies the quantity of an existing order.
func (ob *OrderBook) UpdateOrder(orderID int64, newQuantity int64) error {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	// Verify order exists
	if !ob.orderAccounts.Contains(orderID) {
		return ErrOrderNotFound
	}

	// Update in core order book
	if err := ob.coreBook.ModifyOrder(orderID, newQuantity); err != nil {
		return err
	}

	// Account mapping and self-trade registration remain unchanged
	return nil
}

// GetOrder retrieves order details by ID.
func (ob *OrderBook) GetOrder(orderID int64) (*coreob.Order, error) {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	order := ob.coreBook.GetOrderDetails(orderID)
	if order == nil {
		return nil, ErrOrderNotFound
	}

	return order, nil
}

// GetBestBid returns the best bid price level, or nil if no bids exist.
func (ob *OrderBook) GetBestBid() *coreob.PriceLevel {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	return ob.coreBook.GetBestBid()
}

// GetBestAsk returns the best ask price level, or nil if no asks exist.
func (ob *OrderBook) GetBestAsk() *coreob.PriceLevel {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	return ob.coreBook.GetBestAsk()
}

// GetSpread returns the bid-ask spread.
func (ob *OrderBook) GetSpread() int64 {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	return ob.coreBook.GetSpread()
}

// GetMidPrice returns the mid price.
func (ob *OrderBook) GetMidPrice() int64 {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	return ob.coreBook.GetMidPrice()
}

// GetDepth returns the market depth (top N price levels on each side).
func (ob *OrderBook) GetDepth(levels int) ([]*coreob.PriceLevel, []*coreob.PriceLevel) {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	bids := ob.coreBook.GetTopNBids(levels)
	asks := ob.coreBook.GetTopNAsks(levels)

	return bids, asks
}

// AggregatedLevel represents a price level with aggregated quantity.
// Used for market data snapshots and exchange feeds (M12-03).
type AggregatedLevel struct {
	Price      int64 // Price (integer representation)
	TotalQty   int64 // Total aggregated quantity at this price
	OrderCount int   // Number of orders at this price
}

// GetAggregatedDepth returns aggregated market depth for exchange market data feeds.
// This method aggregates orders at the same price level, which is required for
// exchange market data protocols (requirement M12-03: 价格档位聚合).
//
// Performance: O(n) where n is the number of price levels requested.
//
// Parameters:
//   - levels: Number of price levels to return on each side
//
// Returns:
//   - bids: Aggregated bid levels (sorted descending by price)
//   - asks: Aggregated ask levels (sorted ascending by price)
func (ob *OrderBook) GetAggregatedDepth(levels int) ([]AggregatedLevel, []AggregatedLevel) {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	bidLevels := ob.coreBook.GetTopNBids(levels)
	askLevels := ob.coreBook.GetTopNAsks(levels)

	bids := make([]AggregatedLevel, 0, len(bidLevels))
	for _, level := range bidLevels {
		bids = append(bids, AggregatedLevel{
			Price:      level.Price,
			TotalQty:   level.TotalQty,
			OrderCount: level.OrderCount,
		})
	}

	asks := make([]AggregatedLevel, 0, len(askLevels))
	for _, level := range askLevels {
		asks = append(asks, AggregatedLevel{
			Price:      level.Price,
			TotalQty:   level.TotalQty,
			OrderCount: level.OrderCount,
		})
	}

	return bids, asks
}

// Clear removes all orders from the order book.
func (ob *OrderBook) Clear() {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	ob.coreBook.Clear()
	ob.orderAccounts.Clear()
}

// GetSymbol returns the symbol for this order book.
func (ob *OrderBook) GetSymbol() string {
	return ob.symbol
}

// GetSymbolID returns the symbol ID for this order book.
func (ob *OrderBook) GetSymbolID() uint32 {
	return ob.symbolID
}

// GetAccountID returns the account ID for an order.
func (ob *OrderBook) GetAccountID(orderID int64) (string, bool) {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	return ob.orderAccounts.Get(orderID)
}

// GetAccountMap returns a copy of the account mapping.
func (ob *OrderBook) GetAccountMap() map[int64]string {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	accountMap := make(map[int64]string, ob.orderAccounts.Len())
	ob.orderAccounts.ForEach(func(orderID int64, accountID string) bool {
		accountMap[orderID] = accountID
		return true
	})
	return accountMap
}

// OrderBookStats contains statistics about the order book.
type OrderBookStats struct {
	Symbol      string
	SymbolID    uint32
	BidLevels   int
	AskLevels   int
	TotalOrders int
	BestBid     int64
	BestAsk     int64
	Spread      int64
}

// GetStats returns statistics about the order book.
func (ob *OrderBook) GetStats() OrderBookStats {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	bestBid := ob.coreBook.GetBestBid()
	bestAsk := ob.coreBook.GetBestAsk()

	bestBidPrice := int64(0)
	if bestBid != nil {
		bestBidPrice = bestBid.Price
	}

	bestAskPrice := int64(0)
	if bestAsk != nil {
		bestAskPrice = bestAsk.Price
	}

	return OrderBookStats{
		Symbol:      ob.symbol,
		SymbolID:    ob.symbolID,
		BidLevels:   ob.coreBook.GetBidLevels(),
		AskLevels:   ob.coreBook.GetAskLevels(),
		TotalOrders: ob.coreBook.GetOrderCount(),
		BestBid:     bestBidPrice,
		BestAsk:     bestAskPrice,
		Spread:      ob.coreBook.GetSpread(),
	}
}
