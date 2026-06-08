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
	accountOrders map[string]accountOrderSet // accountID -> order info set
}

// accountOrderInfo tracks essential order information for self-trade detection.
type accountOrderInfo struct {
	OrderID int64
	Side    Side
}

type accountOrderSet struct {
	single accountOrderInfo
	orders map[int64]accountOrderInfo
}

// NewSelfTradeCheck creates a new self-trade checker.
func NewSelfTradeCheck() *SelfTradeCheck {
	return NewSelfTradeCheckWithCapacity(0)
}

// NewSelfTradeCheckWithCapacity creates a new self-trade checker with pre-allocated account capacity.
func NewSelfTradeCheckWithCapacity(capacity int) *SelfTradeCheck {
	return &SelfTradeCheck{
		accountOrders: make(map[string]accountOrderSet, capacity),
	}
}

// RegisterOrder registers an order for self-trade tracking.
// This should be called when an order enters the order book.
func (s *SelfTradeCheck) RegisterOrder(orderID int64, accountID string, side Side) {
	s.mu.Lock()
	defer s.mu.Unlock()

	info := accountOrderInfo{OrderID: orderID, Side: side}
	set, exists := s.accountOrders[accountID]
	if !exists {
		set.single = info
		s.accountOrders[accountID] = set
		return
	}
	if set.orders == nil {
		if set.single.OrderID == orderID {
			set.single = info
			s.accountOrders[accountID] = set
			return
		}
		set.orders = make(map[int64]accountOrderInfo, 2)
		set.orders[set.single.OrderID] = set.single
	}
	set.orders[orderID] = info
	s.accountOrders[accountID] = set
}

// UnregisterOrder removes an order from self-trade tracking.
// This should be called when an order leaves the order book (filled, canceled, etc.).
func (s *SelfTradeCheck) UnregisterOrder(orderID int64, accountID string) {
	s.mu.Lock()
	defer s.mu.Unlock()

	set, exists := s.accountOrders[accountID]
	if !exists {
		return
	}
	if set.orders == nil {
		if set.single.OrderID == orderID {
			delete(s.accountOrders, accountID)
		}
		return
	}

	delete(set.orders, orderID)
	if len(set.orders) == 0 {
		delete(s.accountOrders, accountID)
		return
	}
	if len(set.orders) == 1 {
		for _, info := range set.orders {
			set.single = info
		}
		set.orders = nil
	}
	s.accountOrders[accountID] = set
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

	if orders.orders == nil {
		return []int64{orders.single.OrderID}
	}

	result := make([]int64, 0, len(orders.orders))
	for orderID := range orders.orders {
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

	if orders.orders == nil {
		if orders.single.Side == side {
			return []int64{orders.single.OrderID}
		}
		return nil
	}

	result := make([]int64, 0, len(orders.orders))
	for _, info := range orders.orders {
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

	for accountID := range s.accountOrders {
		delete(s.accountOrders, accountID)
	}
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
		if orders.orders == nil {
			stats.TotalOrders++
			continue
		}
		stats.TotalOrders += len(orders.orders)
	}

	return stats
}
