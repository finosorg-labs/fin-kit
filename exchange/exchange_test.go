package exchange

import (
	"fmt"
	"testing"
)

// TestOrderCreation tests order creation with validation.
func TestOrderCreation(t *testing.T) {
	tests := []struct {
		name      string
		orderID   int64
		accountID string
		price     int64
		quantity  int64
		side      Side
		orderType OrderType
		wantErr   bool
	}{
		{
			name:      "Valid limit order",
			orderID:   1,
			accountID: "ACC1",
			price:     100,
			quantity:  10,
			side:      SideBuy,
			orderType: OrderTypeLimit,
			wantErr:   false,
		},
		{
			name:      "Invalid order ID",
			orderID:   0,
			accountID: "ACC1",
			price:     100,
			quantity:  10,
			side:      SideBuy,
			orderType: OrderTypeLimit,
			wantErr:   true,
		},
		{
			name:      "Invalid account ID",
			orderID:   1,
			accountID: "",
			price:     100,
			quantity:  10,
			side:      SideBuy,
			orderType: OrderTypeLimit,
			wantErr:   true,
		},
		{
			name:      "Invalid quantity",
			orderID:   1,
			accountID: "ACC1",
			price:     100,
			quantity:  0,
			side:      SideBuy,
			orderType: OrderTypeLimit,
			wantErr:   true,
		},
		{
			name:      "Invalid price for limit order",
			orderID:   1,
			accountID: "ACC1",
			price:     0,
			quantity:  10,
			side:      SideBuy,
			orderType: OrderTypeLimit,
			wantErr:   true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			order, err := NewOrder(tt.orderID, tt.accountID, tt.price, tt.quantity, tt.side, tt.orderType)
			if (err != nil) != tt.wantErr {
				t.Errorf("NewOrder() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr && order == nil {
				t.Error("Expected non-nil order")
			}
		})
	}
}

// TestSelfTradeDetection tests self-trade detection functionality.
func TestSelfTradeDetection(t *testing.T) {
	stc := NewSelfTradeCheck()

	// Register orders
	stc.RegisterOrder(1, "ACC1", SideBuy)
	stc.RegisterOrder(2, "ACC1", SideSell)
	stc.RegisterOrder(3, "ACC2", SideBuy)

	tests := []struct {
		name            string
		buyAccountID    string
		sellAccountID   string
		expectSelfTrade bool
	}{
		{
			name:            "Self-trade detected",
			buyAccountID:    "ACC1",
			sellAccountID:   "ACC1",
			expectSelfTrade: true,
		},
		{
			name:            "No self-trade",
			buyAccountID:    "ACC1",
			sellAccountID:   "ACC2",
			expectSelfTrade: false,
		},
		{
			name:            "Empty account IDs",
			buyAccountID:    "",
			sellAccountID:   "",
			expectSelfTrade: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := stc.Check(tt.buyAccountID, tt.sellAccountID)
			if result != tt.expectSelfTrade {
				t.Errorf("Check() = %v, want %v", result, tt.expectSelfTrade)
			}
		})
	}

	// Test unregister
	stc.UnregisterOrder(1, "ACC1")
	orders := stc.GetAccountOrders("ACC1")
	if len(orders) != 1 {
		t.Errorf("Expected 1 order after unregister, got %d", len(orders))
	}
}

// TestTradeReporter tests trade reporting functionality.
func TestTradeReporter(t *testing.T) {
	tr := NewTradeReporter()

	// Create and register an order
	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	tr.RegisterOrder(order)

	sellOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        100,
		Quantity:     10,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}
	tr.RegisterOrder(sellOrder)

	// Create a trade
	trade := &Trade{
		BuyOrderID:    1,
		SellOrderID:   2,
		BuyAccountID:  "ACC1",
		SellAccountID: "ACC2",
		Price:         100,
		Quantity:      5,
	}

	// Record the trade
	buyReport, sellReport, err := tr.RecordTrade(trade)
	if err != nil {
		t.Fatalf("RecordTrade() error = %v", err)
	}

	// Verify buy report
	if buyReport.OrderID != 1 {
		t.Errorf("Expected orderID 1, got %d", buyReport.OrderID)
	}
	if buyReport.FilledQty != 5 {
		t.Errorf("Expected filled qty 5, got %d", buyReport.FilledQty)
	}
	if buyReport.RemainingQty != 5 {
		t.Errorf("Expected remaining qty 5, got %d", buyReport.RemainingQty)
	}
	if buyReport.Status != OrderStatusPartial {
		t.Errorf("Expected status Partial, got %v", buyReport.Status)
	}

	// Verify sell report
	if sellReport == nil {
		t.Fatal("Expected non-nil sell report")
	}

	// Get order report
	report, err := tr.GetOrderReport(1)
	if err != nil {
		t.Fatalf("GetOrderReport() error = %v", err)
	}
	if report.FilledQty != 5 {
		t.Errorf("Expected filled qty 5, got %d", report.FilledQty)
	}
}

// TestMatchingEngine tests the main matching engine functionality.
func TestMatchingEngine(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a limit buy order
	buyOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	report, err := engine.SubmitOrder(buyOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}
	if report.Status != OrderStatusNew {
		t.Errorf("Expected status New, got %v", report.Status)
	}

	// Submit a matching sell order from different account
	sellOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        100,
		Quantity:     5,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 5,
	}

	report, err = engine.SubmitOrder(sellOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}
	if report.Status != OrderStatusFilled {
		t.Errorf("Expected status Filled, got %v", report.Status)
	}
	if report.FilledQty != 5 {
		t.Errorf("Expected filled qty 5, got %d", report.FilledQty)
	}

	// Verify buy order is partially filled
	buyReport, err := engine.GetOrderReport(1)
	if err != nil {
		t.Fatalf("GetOrderReport() error = %v", err)
	}
	if buyReport.Status != OrderStatusPartial {
		t.Errorf("Expected buy order status Partial, got %v", buyReport.Status)
	}
	if buyReport.FilledQty != 5 {
		t.Errorf("Expected buy order filled qty 5, got %d", buyReport.FilledQty)
	}
}

// TestSelfTradePrevent tests that self-trades are prevented.
func TestSelfTradePrevent(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a buy order
	buyOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	_, err := engine.SubmitOrder(buyOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Submit a sell order from same account (should not match due to self-trade prevention)
	sellOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     5,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 5,
	}

	report, err := engine.SubmitOrder(sellOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Sell order should not be filled (no match due to self-trade prevention)
	if report.FilledQty != 0 {
		t.Errorf("Expected filled qty 0 (self-trade prevented), got %d", report.FilledQty)
	}
}

// TestMarketOrder tests market order execution.
func TestMarketOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a limit sell order
	sellOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	_, err := engine.SubmitOrder(sellOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Submit a market buy order
	marketOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        0, // Market orders don't have a price
		Quantity:     5,
		Side:         SideBuy,
		Type:         OrderTypeMarket,
		RemainingQty: 5,
	}

	report, err := engine.SubmitOrder(marketOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	if report.FilledQty != 5 {
		t.Errorf("Expected filled qty 5, got %d", report.FilledQty)
	}
}

// TestIOCOrder tests Immediate-Or-Cancel order execution.
func TestIOCOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a limit sell order with quantity 3
	sellOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     3,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 3,
	}

	_, err := engine.SubmitOrder(sellOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Submit an IOC buy order with quantity 5
	// Should fill 3, cancel remaining 2
	iocOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        100,
		Quantity:     5,
		Side:         SideBuy,
		Type:         OrderTypeIOC,
		RemainingQty: 5,
	}

	report, err := engine.SubmitOrder(iocOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	if report.FilledQty != 3 {
		t.Errorf("Expected filled qty 3, got %d", report.FilledQty)
	}
	if report.RemainingQty != 0 {
		t.Errorf("Expected remaining qty 0 (IOC canceled rest), got %d", report.RemainingQty)
	}
}

// TestFOKOrder tests Fill-Or-Kill order execution.
func TestFOKOrder(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a limit sell order with quantity 3
	sellOrder := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     3,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 3,
	}

	_, err := engine.SubmitOrder(sellOrder)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Submit a FOK buy order with quantity 5
	// Should be rejected (insufficient liquidity)
	fokOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        100,
		Quantity:     5,
		Side:         SideBuy,
		Type:         OrderTypeFOK,
		RemainingQty: 5,
	}

	report, err := engine.SubmitOrder(fokOrder)
	if err == nil {
		t.Error("Expected error for FOK order with insufficient liquidity")
	}
	if report == nil || report.Status != OrderStatusRejected {
		t.Error("Expected rejected status for FOK order")
	}
}

// TestOrderCancellation tests order cancellation.
func TestOrderCancellation(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit a limit buy order
	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10,
	}

	_, err := engine.SubmitOrder(order)
	if err != nil {
		t.Fatalf("SubmitOrder() error = %v", err)
	}

	// Cancel the order
	report, err := engine.CancelOrder("DEFAULT", 1, "User requested")
	if err != nil {
		t.Fatalf("CancelOrder() error = %v", err)
	}

	if report.Status != OrderStatusCanceled {
		t.Errorf("Expected status Canceled, got %v", report.Status)
	}
	if report.RejectReason != "User requested" {
		t.Errorf("Expected reason 'User requested', got %q", report.RejectReason)
	}
}

// TestMatchingEngineStats tests statistics collection.
func TestMatchingEngineStats(t *testing.T) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Submit some orders
	for i := int64(1); i <= 3; i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    "ACC1",
			Price:        100 + i,
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}
	stats := engine.GetStats()
	if stats.TotalActiveOrders != 3 {
		t.Errorf("Expected 3 active orders, got %d", stats.TotalActiveOrders)
	}
	if !stats.Running {
		t.Error("Expected engine to be running")
	}
}

// benchmarks

// BenchmarkSubmitOrder benchmarks order submission to matching engine.
func BenchmarkSubmitOrder(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()
	// Pre-populate with some orders
	for i := int64(1); i <= 100; i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100 + (i % 10),
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	accounts := make([]string, b.N)
	for i := range accounts {
		accounts[i] = fmt.Sprintf("BENCH%d", i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		order := &Order{
			OrderID:      int64(1000 + i),
			AccountID:    accounts[i],
			Price:        95,
			Quantity:     5,
			Side:         SideSell,
			Type:         OrderTypeLimit,
			RemainingQty: 5,
		}
		engine.SubmitOrder(order)
	}
}

// BenchmarkMatchOrder benchmarks order matching performance.
func BenchmarkMatchOrder(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Pre-populate with resting orders
	for i := int64(1); i <= int64(b.N); i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100,
			Quantity:     10,
			Side:         SideSell,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	accounts := make([]string, b.N)
	for i := range accounts {
		accounts[i] = fmt.Sprintf("BENCH%d", i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		order := &Order{
			OrderID:      int64(10000 + i),
			AccountID:    accounts[i],
			Price:        100,
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}
}

func BenchmarkSubmitOrderPreallocated(b *testing.B) {
	engine := NewMatchingEngine(WithMatchingEngineOrderCapacity(b.N + 100))
	defer engine.Close()
	// Pre-populate with some orders
	for i := int64(1); i <= 100; i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100 + (i % 10),
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	accounts := make([]string, b.N)
	for i := range accounts {
		accounts[i] = fmt.Sprintf("BENCH%d", i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		order := &Order{
			OrderID:      int64(1000 + i),
			AccountID:    accounts[i],
			Price:        95,
			Quantity:     5,
			Side:         SideSell,
			Type:         OrderTypeLimit,
			RemainingQty: 5,
		}
		engine.SubmitOrder(order)
	}
}

func BenchmarkMatchOrderPreallocated(b *testing.B) {
	engine := NewMatchingEngine(WithMatchingEngineOrderCapacity(b.N))
	defer engine.Close()

	// Pre-populate with resting orders
	for i := int64(1); i <= int64(b.N); i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100,
			Quantity:     10,
			Side:         SideSell,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	accounts := make([]string, b.N)
	for i := range accounts {
		accounts[i] = fmt.Sprintf("BENCH%d", i)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		order := &Order{
			OrderID:      int64(10000 + i),
			AccountID:    accounts[i],
			Price:        100,
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}
}

// BenchmarkSelfTradeCheck benchmarks self-trade detection.
func BenchmarkSelfTradeCheck(b *testing.B) {
	stc := NewSelfTradeCheck()

	// Register many orders
	for i := int64(1); i <= 10000; i++ {
		side := SideBuy
		if i%2 == 0 {
			side = SideSell
		}
		stc.RegisterOrder(i, fmt.Sprintf("ACC%d", i%100), side)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		stc.Check("ACC1", "ACC2")
	}
}

// BenchmarkGetDepth benchmarks order book depth retrieval.
func BenchmarkGetDepth(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Pre-populate order book
	for i := int64(1); i <= 1000; i++ {
		side := SideBuy
		if i%2 == 0 {
			side = SideSell
		}
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100 + (i % 50),
			Quantity:     10,
			Side:         side,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	ob, _ := engine.GetOrderBook("DEFAULT")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ob.GetDepth(10)
	}
}

// BenchmarkGetAggregatedDepth benchmarks aggregated depth calculation.
func BenchmarkGetAggregatedDepth(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Pre-populate order book
	for i := int64(1); i <= 1000; i++ {
		side := SideBuy
		if i%2 == 0 {
			side = SideSell
		}
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100 + (i % 50),
			Quantity:     10,
			Side:         side,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	ob, _ := engine.GetOrderBook("DEFAULT")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ob.GetAggregatedDepth(10)
	}
}

// BenchmarkTradeReporting benchmarks trade report generation.
func BenchmarkTradeReporting(b *testing.B) {
	tr := NewTradeReporter()

	// Register an order
	order := &Order{
		OrderID:      1,
		AccountID:    "ACC1",
		Price:        100,
		Quantity:     10000,
		Side:         SideBuy,
		Type:         OrderTypeLimit,
		RemainingQty: 10000,
	}
	tr.RegisterOrder(order)

	sellOrder := &Order{
		OrderID:      2,
		AccountID:    "ACC2",
		Price:        100,
		Quantity:     10000,
		Side:         SideSell,
		Type:         OrderTypeLimit,
		RemainingQty: 10000,
	}
	tr.RegisterOrder(sellOrder)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		trade := &Trade{
			BuyOrderID:    1,
			SellOrderID:   2,
			BuyAccountID:  "ACC1",
			SellAccountID: "ACC2",
			Price:         100,
			Quantity:      1,
		}
		tr.RecordTrade(trade)
	}
}

// BenchmarkConcurrentSubmit benchmarks concurrent order submission.
func BenchmarkConcurrentSubmit(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()

	b.RunParallel(func(pb *testing.PB) {
		i := int64(0)
		for pb.Next() {
			i++
			order := &Order{
				OrderID:      i * 100000,
				AccountID:    fmt.Sprintf("ACC%d", i),
				Price:        100,
				Quantity:     10,
				Side:         SideBuy,
				Type:         OrderTypeLimit,
				RemainingQty: 10,
			}
			engine.SubmitOrder(order)
		}
	})
}

// BenchmarkOrderBookStats benchmarks statistics collection.
func BenchmarkOrderBookStats(b *testing.B) {
	engine := NewMatchingEngine()
	defer engine.Close()

	// Pre-populate order book
	for i := int64(1); i <= 100; i++ {
		order := &Order{
			OrderID:      i,
			AccountID:    fmt.Sprintf("ACC%d", i),
			Price:        100,
			Quantity:     10,
			Side:         SideBuy,
			Type:         OrderTypeLimit,
			RemainingQty: 10,
		}
		engine.SubmitOrder(order)
	}

	ob, _ := engine.GetOrderBook("DEFAULT")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ob.GetStats()
	}
}
