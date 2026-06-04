// Package exchange implements self-trade detection for the matching engine.
package exchange

// This file implements self-trade detection to prevent orders from the same account
// from matching against each other.

import (
	"sync"
)

// SelfTradeCheck detects and prevents self-trades (orders from the same account matching).
// Self-trades occur when a buy order and a sell order from the same account would match,
// which is typically prohibited in exchange matching rules.
//
// Concurrency: Thread-safe via mutex.
type SelfTradeCheck struct {
	mu            sync.RWMutex
	accountOrders map[string]map[int64]*accountOrderInfo // accountID -> orderID -> order info
}

// accountOrderInfo tracks essential order information for self-trade detection.
type accountOrderInfo struct {
	OrderID int64
	Side    Side
}

// NewSelfTradeCheck creates a new self-trade checker.
func NewSelfTradeCheck() *SelfTradeCheck {
	return &SelfTradeCheck{
		accountOrders: make(map[string]map[int64]*accountOrderInfo),
	}
}

// RegisterOrder registers an order for self-trade tracking.
// This should be called when an order enters the order book.
func (s *SelfTradeCheck) RegisterOrder(orderID int64, accountID string, side Side) {
	s.mu.Lock()
	defer s.mu.Unlock()

	orders, exists := s.accountOrders[accountID]
	if !exists {
		orders = make(map[int64]*accountOrderInfo)
		s.accountOrders[accountID] = orders
	}

	orders[orderID] = &accountOrderInfo{
		OrderID: orderID,
		Side:    side,
	}
}

// UnregisterOrder removes an order from self-trade tracking.
// This should be called when an order leaves the order book (filled, canceled, etc.).
func (s *SelfTradeCheck) UnregisterOrder(orderID int64, accountID string) {
	s.mu.Lock()
	defer s.mu.Unlock()

	orders, exists := s.accountOrders[accountID]
	if !exists {
		return
	}

	delete(orders, orderID)

	// Clean up empty account entries
	if len(orders) == 0 {
		delete(s.accountOrders, accountID)
	}
}

// Check checks if two orders would result in a self-trade.
// Returns true if both orders belong to the same account (self-trade).
// Returns false if orders are from different accounts (valid match).
//
// Parameters:
//   - buyAccountID: Account ID of the buy side order
//   - sellAccountID: Account ID of the sell side order
//
// Returns:
//   - bool: true if self-trade detected, false otherwise
func (s *SelfTradeCheck) Check(buyAccountID, sellAccountID string) bool {
	return buyAccountID == sellAccountID && buyAccountID != ""
}

// GetAccountOrders returns all order IDs for a specific account.
// This is useful for debugging or auditing purposes.
func (s *SelfTradeCheck) GetAccountOrders(accountID string) []int64 {
	s.mu.RLock()
	defer s.mu.RUnlock()
	orders, exists := s.accountOrders[accountID]
	if !exists {
		return nil
	}

	result := make([]int64, 0, len(orders))
	for orderID := range orders {
		result = append(result, orderID)
	}
	return result
}

// GetAccountOrdersBySide returns all order IDs for a specific account and side.
func (s *SelfTradeCheck) GetAccountOrdersBySide(accountID string, side Side) []int64 {
	s.mu.RLock()
	defer s.mu.RUnlock()

	orders, exists := s.accountOrders[accountID]
	if !exists {
		return nil
	}

	result := make([]int64, 0, len(orders))
	for _, info := range orders {
		if info.Side == side {
			result = append(result, info.OrderID)
		}
	}
	return result
}

// Clear removes all tracked orders.
func (s *SelfTradeCheck) Clear() {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.accountOrders = make(map[string]map[int64]*accountOrderInfo)
}

// Stats returns statistics about tracked orders.
type SelfTradeStats struct {
	TotalAccounts int // Total number of accounts with orders
	TotalOrders   int // Total number of tracked orders
}

// GetStats returns statistics about tracked orders.
func (s *SelfTradeCheck) GetStats() SelfTradeStats {
	s.mu.RLock()
	defer s.mu.RUnlock()

	stats := SelfTradeStats{
		TotalAccounts: len(s.accountOrders),
	}

	for _, orders := range s.accountOrders {
		stats.TotalOrders += len(orders)
	}

	return stats
}
