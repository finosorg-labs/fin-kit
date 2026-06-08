// Package exchange implements trade reporting for the matching engine.
package exchange

// This file implements the trade reporter that generates execution reports
// for matched trades.

import (
	"sync"
	"time"
)

// TradeReporter generates trade execution reports and tracks order fill status.
// It maintains order state and calculates average fill prices.
//
// Concurrency: Thread-safe via mutex.
type TradeReporter struct {
	mu           sync.RWMutex
	orders       map[int64]*trackedOrder // orderID -> tracked order state
	tradeCounter int64                   // Trade ID counter
}

// trackedOrder maintains the state of an order for reporting purposes.
type trackedOrder struct {
	Order        *Order
	Status       OrderStatus
	FilledQty    int64
	RemainingQty int64
	TotalValue   int64 // Sum of (price * quantity) for average price calculation
	Trades       []*Trade
}

// NewTradeReporter creates a new trade reporter.
func NewTradeReporter() *TradeReporter {
	return &TradeReporter{
		orders: make(map[int64]*trackedOrder),
	}
}

// RegisterOrder registers an order for tracking.
// This should be called when an order is submitted to the matching engine.
func (r *TradeReporter) RegisterOrder(order *Order) {
	r.mu.Lock()
	defer r.mu.Unlock()

	r.orders[order.OrderID] = &trackedOrder{
		Order:        order,
		Status:       OrderStatusNew,
		FilledQty:    0,
		RemainingQty: order.Quantity,
		TotalValue:   0,
	}
}

// RecordTrade records a trade and updates order fill status.
// Returns execution reports for both buy and sell sides.
func (r *TradeReporter) RecordTrade(trade *Trade) (*ExecutionReport, *ExecutionReport, error) {
	r.mu.Lock()
	defer r.mu.Unlock()

	// Generate unique trade ID
	r.tradeCounter++
	trade.TradeID = r.tradeCounter
	trade.Timestamp = time.Now().UnixNano()

	// Update buy side
	buyReport, err := r.updateOrderState(trade.BuyOrderID, trade.Quantity, trade.Price, trade)
	if err != nil {
		return nil, nil, err
	}

	// Update sell side
	sellReport, err := r.updateOrderState(trade.SellOrderID, trade.Quantity, trade.Price, trade)
	if err != nil {
		return nil, nil, err
	}

	// Set fill types in trade
	trade.BuyFillType = r.getFillType(buyReport)
	trade.SellFillType = r.getFillType(sellReport)

	return buyReport, sellReport, nil
}

// updateOrderState updates the order state after a trade.
func (r *TradeReporter) updateOrderState(orderID int64, qty int64, price int64, trade *Trade) (*ExecutionReport, error) {
	tracked, exists := r.orders[orderID]
	if !exists {
		return nil, ErrOrderNotFound
	}

	// Update fill quantities
	tracked.FilledQty += qty
	tracked.RemainingQty -= qty
	tracked.TotalValue += price * qty
	tracked.Trades = append(tracked.Trades, trade)

	// Sync tracked state to original Order object
	tracked.Order.FilledQty = tracked.FilledQty
	tracked.Order.RemainingQty = tracked.RemainingQty

	// Update status
	if tracked.RemainingQty <= 0 {
		tracked.Status = OrderStatusFilled
	} else {
		tracked.Status = OrderStatusPartial
	}

	// Calculate average price
	avgPrice := 0.0
	if tracked.FilledQty > 0 {
		avgPrice = float64(tracked.TotalValue) / float64(tracked.FilledQty)
	}

	return &ExecutionReport{
		OrderID:      orderID,
		AccountID:    tracked.Order.AccountID,
		Status:       tracked.Status,
		FilledQty:    tracked.FilledQty,
		RemainingQty: tracked.RemainingQty,
		AvgPrice:     avgPrice,
		Trades:       tracked.Trades,
		Timestamp:    time.Now().UnixNano(),
	}, nil
}

// CancelOrder marks an order as canceled and generates an execution report.
func (r *TradeReporter) CancelOrder(orderID int64, reason string) (*ExecutionReport, error) {
	r.mu.Lock()
	defer r.mu.Unlock()

	tracked, exists := r.orders[orderID]
	if !exists {
		return nil, ErrOrderNotFound
	}

	tracked.Status = OrderStatusCanceled

	// Calculate average price if partially filled
	avgPrice := 0.0
	if tracked.FilledQty > 0 {
		avgPrice = float64(tracked.TotalValue) / float64(tracked.FilledQty)
	}

	return &ExecutionReport{
		OrderID:      orderID,
		AccountID:    tracked.Order.AccountID,
		Status:       OrderStatusCanceled,
		FilledQty:    tracked.FilledQty,
		RemainingQty: tracked.RemainingQty,
		AvgPrice:     avgPrice,
		Trades:       tracked.Trades,
		RejectReason: reason,
		Timestamp:    time.Now().UnixNano(),
	}, nil
}

// RejectOrder marks an order as rejected and generates an execution report.
func (r *TradeReporter) RejectOrder(orderID int64, reason string) (*ExecutionReport, error) {
	r.mu.Lock()
	defer r.mu.Unlock()

	tracked, exists := r.orders[orderID]
	if !exists {
		// Order was never registered, create minimal report
		return &ExecutionReport{
			OrderID:      orderID,
			Status:       OrderStatusRejected,
			FilledQty:    0,
			RemainingQty: 0,
			AvgPrice:     0,
			Trades:       nil,
			RejectReason: reason,
			Timestamp:    time.Now().UnixNano(),
		}, nil
	}

	tracked.Status = OrderStatusRejected

	return &ExecutionReport{
		OrderID:      orderID,
		AccountID:    tracked.Order.AccountID,
		Status:       OrderStatusRejected,
		FilledQty:    0,
		RemainingQty: tracked.Order.Quantity,
		AvgPrice:     0,
		Trades:       nil,
		RejectReason: reason,
		Timestamp:    time.Now().UnixNano(),
	}, nil
}

// GetOrderReport returns the current execution report for an order.
func (r *TradeReporter) GetOrderReport(orderID int64) (*ExecutionReport, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	tracked, exists := r.orders[orderID]
	if !exists {
		return nil, ErrOrderNotFound
	}

	// Calculate average price
	avgPrice := 0.0
	if tracked.FilledQty > 0 {
		avgPrice = float64(tracked.TotalValue) / float64(tracked.FilledQty)
	}

	return &ExecutionReport{
		OrderID:      orderID,
		AccountID:    tracked.Order.AccountID,
		Status:       tracked.Status,
		FilledQty:    tracked.FilledQty,
		RemainingQty: tracked.RemainingQty,
		AvgPrice:     avgPrice,
		Trades:       tracked.Trades,
		Timestamp:    time.Now().UnixNano(),
	}, nil
}

// UnregisterOrder removes an order from tracking.
// This should be called after an order is completely processed (filled or canceled).
func (r *TradeReporter) UnregisterOrder(orderID int64) {
	r.mu.Lock()
	defer r.mu.Unlock()

	delete(r.orders, orderID)
}

// UpdateRemainingQty updates the remaining quantity for an order.
// This is used for IOC/Market orders where remaining quantity is canceled.
func (r *TradeReporter) UpdateRemainingQty(orderID int64, remaining int64, canceled int64) error {
	r.mu.Lock()
	defer r.mu.Unlock()

	tracked, exists := r.orders[orderID]
	if !exists {
		return ErrOrderNotFound
	}

	tracked.RemainingQty = remaining
	tracked.Order.CanceledQty = canceled
	tracked.Order.RemainingQty = remaining

	return nil
}

// Clear removes all tracked orders.
func (r *TradeReporter) Clear() {
	r.mu.Lock()
	defer r.mu.Unlock()

	for orderID := range r.orders {
		delete(r.orders, orderID)
	}
	r.tradeCounter = 0
}

// getFillType determines the fill type from an execution report.
func (r *TradeReporter) getFillType(report *ExecutionReport) FillType {
	switch report.Status {
	case OrderStatusFilled:
		return FillTypeComplete
	case OrderStatusPartial:
		return FillTypePartial
	case OrderStatusCanceled:
		return FillTypeCanceled
	default:
		return FillTypePartial
	}
}

// TradeReporterStats contains statistics about tracked orders and trades.
type TradeReporterStats struct {
	TotalOrders     int
	ActiveOrders    int
	FilledOrders    int
	CanceledOrders  int
	RejectedOrders  int
	TotalTrades     int64
}

// GetStats returns statistics about tracked orders and trades.
func (r *TradeReporter) GetStats() TradeReporterStats {
	r.mu.RLock()
	defer r.mu.RUnlock()

	stats := TradeReporterStats{
		TotalOrders: len(r.orders),
		TotalTrades: r.tradeCounter,
	}

	for _, tracked := range r.orders {
		switch tracked.Status {
		case OrderStatusNew, OrderStatusPartial:
			stats.ActiveOrders++
		case OrderStatusFilled:
			stats.FilledOrders++
		case OrderStatusCanceled:
			stats.CanceledOrders++
		case OrderStatusRejected:
			stats.RejectedOrders++
		}
	}

	return stats
}
