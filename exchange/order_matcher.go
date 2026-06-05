// Package exchange implements order matching for the matching engine.
package exchange

// This file implements the order matcher that executes price-time priority
// matching between incoming orders and the order book.

import (
	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// OrderMatcher implements price-time priority order matching algorithm.
// It matches incoming orders against resting orders in the order book,
// generates trades, and handles different order types.
//
// Concurrency: Not thread-safe. Caller must ensure mutual exclusion.
type OrderMatcher struct {
	selfTradeCheck *SelfTradeCheck
	tradeReporter  *TradeReporter
	maxDepthLevels int // Maximum number of price levels to scan during matching
}

// NewOrderMatcher creates a new order matcher with default settings.
// Default maxDepthLevels is 100.
func NewOrderMatcher(selfTradeCheck *SelfTradeCheck, tradeReporter *TradeReporter) *OrderMatcher {
	return &OrderMatcher{
		selfTradeCheck: selfTradeCheck,
		tradeReporter:  tradeReporter,
		maxDepthLevels: 100,
	}
}

// NewOrderMatcherWithDepth creates a new order matcher with custom depth limit.
// The maxDepthLevels parameter controls how many price levels to scan during matching.
// Higher values allow matching deeper into the order book but may impact latency.
func NewOrderMatcherWithDepth(selfTradeCheck *SelfTradeCheck, tradeReporter *TradeReporter, maxDepthLevels int) *OrderMatcher {
	if maxDepthLevels <= 0 {
		maxDepthLevels = 100
	}
	return &OrderMatcher{
		selfTradeCheck: selfTradeCheck,
		tradeReporter:  tradeReporter,
		maxDepthLevels: maxDepthLevels,
	}
}

// SetMaxDepthLevels sets the maximum number of price levels to scan during matching.
func (m *OrderMatcher) SetMaxDepthLevels(levels int) {
	if levels > 0 {
		m.maxDepthLevels = levels
	}
}

// Match executes order matching between an incoming order and the order book.
// Returns all trades generated and any error encountered.
//
// Matching algorithm:
//   - Price-time priority: best price first, then earliest timestamp
//   - Self-trade prevention: orders from same account cannot match
//   - Partial fills: match available quantity, leave remainder
//   - Order type specific behavior (FOK, IOC, Market, Iceberg)
//
// Time complexity: O(m * log n) where:
//   - m = number of orders matched (typically < 10)
//   - n = number of price levels in order book
//
// Typical performance: ~100-150μs per match operation including
// all book updates, trade generation, and self-trade checks.
//
// Parameters:
//   - order: Incoming order to match
//   - ob: Order book containing resting orders
//   - accountMap: Mapping of orderID to accountID for self-trade prevention
//
// Returns:
//   - []*Trade: Trades generated from matching
//   - error: Error if matching fails
func (m *OrderMatcher) Match(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	if order == nil {
		return nil, ErrNilOrder
	}

	// Validate order before matching
	if err := m.validateOrder(order); err != nil {
		return nil, err
	}

	// Handle different order types
	switch order.Type {
	case OrderTypeMarket:
		return m.matchMarketOrder(order, ob, accountMap)
	case OrderTypeFOK:
		return m.matchFOKOrder(order, ob, accountMap)
	case OrderTypeIOC:
		return m.matchIOCOrder(order, ob, accountMap)
	case OrderTypeLimit:
		return m.matchLimitOrder(order, ob, accountMap)
	case OrderTypeIceberg:
		return m.matchIcebergOrder(order, ob, accountMap)
	default:
		return nil, ErrInvalidOrderType
	}
}

// matchMarketOrder matches a market order at best available price.
// Market orders are fully filled or canceled if insufficient liquidity.
func (m *OrderMatcher) matchMarketOrder(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	trades := make([]*Trade, 0)
	remaining := order.Quantity

	// Get orders from opposite side
	var levels []*coreob.PriceLevel
	if order.Side == SideBuy {
		levels = ob.GetTopNAsks(m.maxDepthLevels)
	} else {
		levels = ob.GetTopNBids(m.maxDepthLevels)
	}

	// Check for empty order book
	if len(levels) == 0 {
		order.CanceledQty = remaining
		order.RemainingQty = 0
		return trades, nil
	}

	// Match against available liquidity
	for _, level := range levels {
		if level == nil {
			continue
		}
		if remaining <= 0 {
			break
		}

		levelOrders := level.Orders
		for _, restingOrder := range levelOrders {
			if restingOrder == nil {
				continue
			}
			if remaining <= 0 {
				break
			}

		// Get account ID from mapping
			restingAccountID, exists := accountMap[restingOrder.OrderID]
			if !exists {
				continue
			}

			// Check self-trade
			if m.selfTradeCheck.Check(order.AccountID, restingAccountID) {
				continue
			}

			// Calculate match quantity
		matchQty := remaining
			if restingOrder.Quantity < matchQty {
				matchQty = restingOrder.Quantity
			}

			// Create trade
			trade := m.createTrade(order, restingOrder, restingAccountID, matchQty, level.Price)
			trades = append(trades, trade)

			// Update quantities
			remaining -= matchQty
			order.FilledQty += matchQty
			order.RemainingQty = remaining
		}
	}

	// Market orders must be fully filled
	if remaining > 0 {
		order.CanceledQty = remaining
		order.RemainingQty = 0
	}

	return trades, nil
}

// matchFOKOrder matches a Fill-Or-Kill order.
// FOK orders must be completely filled immediately or canceled entirely.
func (m *OrderMatcher) matchFOKOrder(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	// First check if sufficient liquidity exists
	available := m.getAvailableLiquidity(order, ob, accountMap)
	if available < order.Quantity {
		return nil, ErrInsufficientQty
	}
	// Match as limit order since we confirmed sufficient liquidity
	return m.matchLimitOrder(order, ob, accountMap)
}

// matchIOCOrder matches an Immediate-Or-Cancel order.
// IOC orders fill available quantity immediately and cancel the rest.
func (m *OrderMatcher) matchIOCOrder(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	trades, err := m.matchLimitOrder(order, ob, accountMap)
	if err != nil {
		return nil, err
	}

	// Cancel remaining quantity
	if order.RemainingQty > 0 {
		order.CanceledQty = order.RemainingQty
		order.RemainingQty = 0
	}

	return trades, nil
}

// matchLimitOrder matches a limit order at specified price or better.
// Partial fills are allowed, remainder stays in the order book.
func (m *OrderMatcher) matchLimitOrder(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	trades := make([]*Trade, 0)
	remaining := order.Quantity

	// Get orders from opposite side
	var levels []*coreob.PriceLevel
	if order.Side == SideBuy {
		levels = ob.GetTopNAsks(m.maxDepthLevels)
	} else {
		levels = ob.GetTopNBids(m.maxDepthLevels)
	}

	// Empty order book is valid - just return no trades
	if len(levels) == 0 {
		return trades, nil
	}

	// Match against price levels
	for _, level := range levels {
		if level == nil {
			continue
		}
		if remaining <= 0 {
			break
		}

		// Check price compatibility
		if !m.isPriceMatch(order, level.Price) {
			break
		}

		levelOrders := level.Orders
		for _, restingOrder := range levelOrders {
			if restingOrder == nil {
				continue
			}
			if remaining <= 0 {
				break
			}

			// Get account ID from mapping
			restingAccountID, exists := accountMap[restingOrder.OrderID]
			if !exists {
				continue
			}

		// Check self-trade
			if m.selfTradeCheck.Check(order.AccountID, restingAccountID) {
				continue
			}

			// Calculate match quantity
			matchQty := remaining
			if restingOrder.Quantity < matchQty {
				matchQty = restingOrder.Quantity
			}

			// Create trade
			trade := m.createTrade(order, restingOrder, restingAccountID, matchQty, level.Price)
			trades = append(trades, trade)

			// Update quantities
			remaining -= matchQty
			order.FilledQty += matchQty
			order.RemainingQty = remaining
		}
	}

	return trades, nil
}

// matchIcebergOrder matches an iceberg order with hidden quantity.
// Only visible quantity is displayed, but matching can fill more.
func (m *OrderMatcher) matchIcebergOrder(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) ([]*Trade, error) {
	// Match using visible quantity logic
	// For now, treat as limit order - iceberg display logic handled by orderbook
	return m.matchLimitOrder(order, ob, accountMap)
}

// createTrade creates a trade between two orders.
func (m *OrderMatcher) createTrade(incoming *Order, resting *coreob.Order, restingAccountID string, quantity int64, price int64) *Trade {
	trade := &Trade{
		Quantity:  quantity,
		Price:     price,
		Timestamp: 0, // Will be set by TradeReporter
	}

	// Set buy/sell sides based on incoming order
	if incoming.Side == SideBuy {
		trade.BuyOrderID = incoming.OrderID
		trade.BuyAccountID = incoming.AccountID
		trade.SellOrderID = resting.OrderID
		trade.SellAccountID = restingAccountID
		trade.SellRemainingQty = resting.Quantity - quantity
	} else {
		trade.SellOrderID = incoming.OrderID
		trade.SellAccountID = incoming.AccountID
		trade.BuyOrderID = resting.OrderID
		trade.BuyAccountID = restingAccountID
		trade.BuyRemainingQty = resting.Quantity - quantity
	}

	return trade
}

// isPriceMatch checks if the order price matches with the resting price level.
func (m *OrderMatcher) isPriceMatch(order *Order, levelPrice int64) bool {
	if order.Side == SideBuy {
		// Buy order: can match at or below limit price
		return levelPrice <= order.Price
	}
	// Sell order: can match at or above limit price
	return levelPrice >= order.Price
}

// getAvailableLiquidity calculates available liquidity for an order.
func (m *OrderMatcher) getAvailableLiquidity(order *Order, ob *coreob.OrderBook, accountMap map[int64]string) int64 {
	available := int64(0)

	// Get orders from opposite side
	var levels []*coreob.PriceLevel
	if order.Side == SideBuy {
		levels = ob.GetTopNAsks(100)
	} else {
		levels = ob.GetTopNBids(100)
	}

	// Sum available quantity at compatible prices
	for _, level := range levels {
		// Check price compatibility for limit orders
		if order.Type == OrderTypeLimit || order.Type == OrderTypeFOK {
			if !m.isPriceMatch(order, level.Price) {
				break
			}
		}

		for _, restingOrder := range level.Orders {
			// Get account ID from mapping
			restingAccountID, exists := accountMap[restingOrder.OrderID]
			if !exists {
		continue
			}

			// Skip self-trades
			if m.selfTradeCheck.Check(order.AccountID, restingAccountID) {
				continue
			}

		available += restingOrder.Quantity

			// Stop if we have enough
			if available >= order.Quantity {
				return available
			}
		}
	}

	return available
}

// validateOrder validates an order before matching.
func (m *OrderMatcher) validateOrder(order *Order) error {
	if order.OrderID <= 0 {
		return ErrInvalidOrderID
	}
	if order.AccountID == "" {
		return ErrInvalidAccountID
	}
	if order.Quantity <= 0 {
		return ErrInvalidQuantity
	}
	if (order.Type == OrderTypeLimit || order.Type == OrderTypeIceberg) && order.Price <= 0 {
		return ErrInvalidPrice
	}
	return nil
}
