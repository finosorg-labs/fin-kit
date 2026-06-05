package exchange

import (
	"testing"
)

// TestEmptyOrderBook tests operations on an empty order book.
func TestEmptyOrderBook(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit market order to empty book - should be canceled
	marketOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        0,
		Quantity:     10,
		Side:       SideBuy,
		Type:         OrderTypeMarket,
		RemainingQty: 10,
	}

	report, err := engine.SubmitOrder(marketOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	if report.FilledQty != 0 {
		t.Errorf("Expected filled qty 0 (empty book), got %d", report.FilledQty)
	}
	if marketOrder.CanceledQty != 10 {
		t.Errorf("Expected canceled qty 10, got %d", marketOrder.CanceledQty)
	}
}

// TestGetDepthEmptyBook tests GetDepth on empty order book.
func TestGetDepthEmptyBook(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()
	// Create order book by submitting and canceling an order
	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	engine.SubmitOrder(order)
	engine.CancelOrder("DEFAULT", 1, "test")

	ob, err := engine.GetOrderBook("DEFAULT")
	if err != nil {
		t.Fatalf("GetOrderBook() error = %v", err)
	}

	// GetDepth should return empty slices, not nil or panic
	bids, asks := ob.GetDepth(10)
	if bids == nil {
		t.Error("Expected non-nil bids slice")
	}
	if asks == nil {
		t.Error("Expected non-nil asks slice")
	}
	if len(bids) != 0 {
		t.Errorf("Expected 0 bids, got %d", len(bids))
	}
	if len(asks) != 0 {
		t.Errorf("Expected 0 asks, got %d", len(asks))
	}
}

// TestGetAggregatedDepth tests aggregated depth calculation.
func TestGetAggregatedDepth(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Add multiple orders at same price
	orders := []*Order{
		{OrderID: 1, AccountID: "ACC1", Price: 100, Quantity: 10, Side: SideBuy, Type: OrderTypeLimit, RemainingQty: 10},
		{OrderID: 2, AccountID: "ACC2", Price: 100, Quantity: 20, Side: SideBuy, Type: OrderTypeLimit, RemainingQty: 20},
		{OrderID: 3, AccountID: "ACC3", Price: 99, Quantity: 15, Side: SideBuy, Type: OrderTypeLimit, RemainingQty: 15},
		{OrderID: 4, AccountID: "ACC4", Price: 101, Quantity: 5, Side: SideSell, Type: OrderTypeLimit, RemainingQty: 5},
		{OrderID: 5, AccountID: "ACC5", Price: 101, Quantity: 8, Side: SideSell, Type: OrderTypeLimit, RemainingQty: 8},
	}

	for _, order := range orders {
		_, err := engine.SubmitOrder(order)
		if err != nil {
			t.Fatalf("SubmitOrder() error = %v", err)
		}
	}

	ob, _ := engine.GetOrderBook("DEFAULT")
	bids, asks := ob.GetAggregatedDepth(10)

	// Check bids
	if len(bids) != 2 {
		t.Errorf("Expected 2 bid levels, got %d", len(bids))
	}
	if len(bids) > 0 && bids[0].Price != 100 {
		t.Errorf("Expected best bid at 100, got %d", bids[0].Price)
	}
	if len(bids) > 0 && bids[0].TotalQty != 30 {
		t.Errorf("Expected aggregated qty 30 at price 100, got %d", bids[0].TotalQty)
	}
	if len(bids) > 0 && bids[0].OrderCount != 2 {
		t.Errorf("Expected 2 orders at price 100, got %d", bids[0].OrderCount)
	}

	// Check asks
	if len(asks) != 1 {
		t.Errorf("Expected 1 ask level, got %d", len(asks))
	}
	if len(asks) > 0 && asks[0].TotalQty != 13 {
		t.Errorf("Expected aggregated qty 13 at price 101, got %d", asks[0].TotalQty)
	}
}

// TestNilOrderProtection tests that nil orders are handled gracefully.
func TestNilOrderProtection(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	_, err := engine.SubmitOrder(nil)
	if err != ErrNilOrder {
		t.Errorf("Expected ErrNilOrder, got %v", err)
	}
}

// TestZeroQuantityOrder tests that zero quantity orders are rejected.
func TestZeroQuantityOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     0,
		Side:      SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 0,
	}

	report, _ := engine.SubmitOrder(order)
	if report == nil {
		t.Fatal("Expected non-nil report")
	}
	if report.Status != OrderStatusRejected {
		t.Errorf("Expected OrderStatusRejected, got %v", report.Status)
	}
	if report.RejectReason == "" {
		t.Error("Expected non-empty reject reason")
	}
}

// TestNegativePriceOrder tests that negative price orders are rejected.
func TestNegativePriceOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        -100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	report, _ := engine.SubmitOrder(order)
	if report == nil {
		t.Fatal("Expected non-nil report")
	}
	if report.Status != OrderStatusRejected {
		t.Errorf("Expected OrderStatusRejected, got %v", report.Status)
	}
	if report.RejectReason == "" {
		t.Error("Expected non-empty reject reason")
	}
}

// TestCancelNonexistentOrder tests canceling an order that doesn't exist.
func TestCancelNonexistentOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit an order to create the order book
	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}
	engine.SubmitOrder(order)

	// Try to cancel a non-existent order
	_, err := engine.CancelOrder("DEFAULT", 999, "test")
	if err == nil {
		t.Error("Expected error when canceling non-existent order")
	}
}

// TestGetBestBidAskEmpty tests getting best bid/ask from empty book.
func TestGetBestBidAskEmpty(t *testing.T) {
	stc := NewSelfTradeCheck()
	ob := NewOrderBook("TEST", 1, stc)

	bestBid := ob.GetBestBid()
	bestAsk := ob.GetBestAsk()

	if bestBid != nil {
		t.Error("Expected nil best bid on empty book")
	}
	if bestAsk != nil {
		t.Error("Expected nil best ask on empty book")
	}

	// Spread should be 0 for empty book
	spread := ob.GetSpread()
	if spread != 0 {
		t.Errorf("Expected spread 0, got %d", spread)
	}
}

// TestOrderMatcherDepthConfiguration tests depth limit configuration.
func TestOrderMatcherDepthConfiguration(t *testing.T) {
	stc := NewSelfTradeCheck()
	tr := NewTradeReporter()

	// Test default depth
	m1 := NewOrderMatcher(stc, tr)
	if m1.maxDepthLevels != 100 {
		t.Errorf("Expected default depth 100, got %d", m1.maxDepthLevels)
	}

	// Test custom depth
	m2 := NewOrderMatcherWithDepth(stc, tr, 50)
	if m2.maxDepthLevels != 50 {
		t.Errorf("Expected depth 50, got %d", m2.maxDepthLevels)
	}

	// Test invalid depth (should default to 100)
	m3 := NewOrderMatcherWithDepth(stc, tr, 0)
	if m3.maxDepthLevels != 100 {
		t.Errorf("Expected default depth 100 for invalid input, got %d", m3.maxDepthLevels)
	}

	// Test SetMaxDepthLevels
	m1.SetMaxDepthLevels(200)
	if m1.maxDepthLevels != 200 {
		t.Errorf("Expected depth 200 after set, got %d", m1.maxDepthLevels)
	}

	// Test SetMaxDepthLevels with invalid value
	m1.SetMaxDepthLevels(-1)
	if m1.maxDepthLevels != 200 {
		t.Errorf("Expected depth unchanged (200) for invalid set, got %d", m1.maxDepthLevels)
	}
}
