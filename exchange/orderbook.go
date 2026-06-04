// Package exchange implements the exchange-specific order book wrapper.
package exchange

// This file implements an order book wrapper that extends the core orderbook
// with account tracking for self-trade prevention.

import (
	"sync"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// OrderBook wraps the core order book with exchange-specific functionality.
// It manages order lifecycle, account tracking, and integrates with self-trade
// prevention and trade reporting.
//
// Concurrency: Thread-safe via mutex.
type OrderBook struct {
	mu           sync.RWMutex
	coreBook     *coreob.OrderBook
	symbol         string
	symbolID       uint32
	selfTradeCheck *SelfTradeCheck
	orderAccounts  map[int64]string // orderID -> accountID mapping
}

// NewOrderBook creates a new exchange order book.
func NewOrderBook(symbol string, symbolID uint32, selfTradeCheck *SelfTradeCheck) *OrderBook {
	return &OrderBook{
		coreBook:       coreob.NewOrderBook(symbol, symbolID),
		symbol:         symbol,
		symbolID:       symbolID,
		selfTradeCheck: selfTradeCheck,
		orderAccounts:  make(map[int64]string),
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
	ob.orderAccounts[order.OrderID] = order.AccountID

	// Register for self-trade prevention
	ob.selfTradeCheck.RegisterOrder(order.OrderID, order.AccountID, order.Side)

	return nil
}

// RemoveOrder removes an order from the order book.
func (ob *OrderBook) RemoveOrder(orderID int64) error {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	// Get account ID before removal
	accountID, exists := ob.orderAccounts[orderID]
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
	delete(ob.orderAccounts, orderID)

	return nil
}

// UpdateOrder updates an existing order in the order book.
// This modifies the quantity of an existing order.
func (ob *OrderBook) UpdateOrder(orderID int64, newQuantity int64) error {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	// Verify order exists
	_, exists := ob.orderAccounts[orderID]
	if !exists {
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

// Clear removes all orders from the order book.
func (ob *OrderBook) Clear() {
	ob.mu.Lock()
	defer ob.mu.Unlock()

	ob.coreBook.Clear()
	ob.orderAccounts = make(map[int64]string)
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

	accountID, exists := ob.orderAccounts[orderID]
	return accountID, exists
}

// GetAccountMap returns a copy of the account mapping for matching.
func (ob *OrderBook) GetAccountMap() map[int64]string {
	ob.mu.RLock()
	defer ob.mu.RUnlock()

	accountMap := make(map[int64]string, len(ob.orderAccounts))
	for orderID, accountID := range ob.orderAccounts {
		accountMap[orderID] = accountID
	}
	return accountMap
}

// OrderBookStats contains statistics about the order book.
type OrderBookStats struct {
	Symbol       string
	SymbolID     uint32
	BidLevels    int
	AskLevels    int
	TotalOrders  int
	BestBid      int64
	BestAsk      int64
	Spread       int64
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
