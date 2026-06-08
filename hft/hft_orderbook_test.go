package hft

import (
	"testing"
	"time"
)

func TestNewHFTOrderBook(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	if ob == nil {
		t.Fatal("NewHFTOrderBook returned nil")
	}
	if ob.core == nil {
		t.Error("core order book is nil")
	}
	if ob.imbalance == nil {
		t.Error("imbalance calculator is nil")
	}
	if ob.liquidity == nil {
		t.Error("liquidity analyzer is nil")
	}
	if ob.signalGen == nil {
		t.Error("signal generator is nil")
	}
	if ob.position == nil {
		t.Error("position tracker is nil")
	}
	if ob.risk == nil {
		t.Error("risk monitor is nil")
	}
	defer ob.Close()
}

func TestHFTOrderBook_OnOrderUpdate_Insert(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	update := &OrderUpdate{
		Type:      OrderUpdateInsert,
		OrderID:   1,
		Price:     1000,
		Quantity:  100,
		Side:      SideBuy,
		Timestamp: time.Now().UnixNano(),
	}

	signal, err := ob.OnOrderUpdate(update)
	if err != nil {
		t.Fatalf("OnOrderUpdate failed: %v", err)
	}
	if signal == nil {
		t.Fatal("signal is nil")
	}
}

func TestHFTOrderBook_OnOrderUpdate_Delete(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	// Insert order first
	insertUpdate := &OrderUpdate{
		Type:      OrderUpdateInsert,
		OrderID:   1,
		Price:     1000,
		Quantity:  100,
		Side:      SideBuy,
		Timestamp: time.Now().UnixNano(),
	}
	_, err := ob.OnOrderUpdate(insertUpdate)
	if err != nil {
		t.Fatalf("insert failed: %v", err)
	}

	// Delete order
	deleteUpdate := &OrderUpdate{
		Type:      OrderUpdateDelete,
		OrderID:   1,
		Timestamp: time.Now().UnixNano(),
	}
	signal, err := ob.OnOrderUpdate(deleteUpdate)
	if err != nil {
		t.Fatalf("delete failed: %v", err)
	}
	if signal == nil {
		t.Fatal("signal is nil")
	}
}

func TestHFTOrderBook_OnOrderUpdate_Modify(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	// Insert order first
	insertUpdate := &OrderUpdate{
		Type:      OrderUpdateInsert,
		OrderID:   1,
		Price:     1000,
		Quantity:  100,
		Side:      SideBuy,
		Timestamp: time.Now().UnixNano(),
	}
	_, err := ob.OnOrderUpdate(insertUpdate)
	if err != nil {
		t.Fatalf("insert failed: %v", err)
	}

	// Modify order
	modifyUpdate := &OrderUpdate{
		Type:      OrderUpdateModify,
		OrderID:   1,
		Quantity:  200,
		Timestamp: time.Now().UnixNano(),
	}
	signal, err := ob.OnOrderUpdate(modifyUpdate)
	if err != nil {
		t.Fatalf("modify failed: %v", err)
	}
	if signal == nil {
		t.Fatal("signal is nil")
	}
}

func TestHFTOrderBook_GetImbalance(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	// Insert orders
	updates := []*OrderUpdate{
		{Type: OrderUpdateInsert, OrderID: 1, Price: 1000, Quantity: 100, Side: SideBuy, Timestamp: time.Now().UnixNano()},
		{Type: OrderUpdateInsert, OrderID: 2, Price: 1001, Quantity: 50, Side: SideSell, Timestamp: time.Now().UnixNano()},
	}

	for _, u := range updates {
		_, err := ob.OnOrderUpdate(u)
		if err != nil {
			t.Fatalf("OnOrderUpdate failed: %v", err)
		}
	}

	metrics := ob.GetImbalance()
	if metrics == nil {
		t.Fatal("GetImbalance returned nil")
	}
}

func TestHFTOrderBook_GetLiquidity(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	// Insert orders
	updates := []*OrderUpdate{
		{Type: OrderUpdateInsert, OrderID: 1, Price: 1000, Quantity: 100, Side: SideBuy, Timestamp: time.Now().UnixNano()},
		{Type: OrderUpdateInsert, OrderID: 2, Price: 1001, Quantity: 50, Side: SideSell, Timestamp: time.Now().UnixNano()},
	}

	for _, u := range updates {
		_, err := ob.OnOrderUpdate(u)
		if err != nil {
			t.Fatalf("OnOrderUpdate failed: %v", err)
		}
	}

	metrics := ob.GetLiquidity()
	if metrics == nil {
		t.Fatal("GetLiquidity returned nil")
	}
	if metrics.Spread <= 0 {
		t.Error("spread should be positive")
	}
}

func TestHFTOrderBook_GetSignal(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	signal := ob.GetSignal()
	if signal == nil {
		t.Fatal("GetSignal returned nil")
	}
	if signal.Type != SignalHold {
		t.Errorf("expected SignalHold, got %v", signal.Type)
	}
}

func TestHFTOrderBook_CheckRisk(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	metrics, err := ob.CheckRisk()
	if err != nil {
		t.Fatalf("CheckRisk failed: %v", err)
	}
	if metrics == nil {
		t.Fatal("CheckRisk returned nil")
	}
}

func TestHFTOrderBook_NilUpdate(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	_, err := ob.OnOrderUpdate(nil)
	if err != ErrNilOrderUpdate {
		t.Errorf("expected ErrNilOrderUpdate, got %v", err)
	}
}

func TestHFTOrderBook_Close(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)

	err := ob.Close()
	if err != nil {
		t.Fatalf("Close failed: %v", err)
	}
}

func TestHFTOrderBook_GetCore(t *testing.T) {
	ob := NewHFTOrderBook("TEST", 1, 0.01)
	defer ob.Close()

	core := ob.GetCore()
	if core == nil {
		t.Fatal("GetCore returned nil")
	}
}

func seedBenchmarkHFTOrderBook(b *testing.B, levels int) *HFTOrderBook {
	b.Helper()

	ob := NewHFTOrderBook("BENCH", 1, 0.01)
	for i := 0; i < levels; i++ {
		if _, err := ob.OnOrderUpdate(&OrderUpdate{
			Type:      OrderUpdateInsert,
			OrderID:   int64(i + 1),
			Price:     int64(100000 - i),
			Quantity:  100,
			Side:      SideBuy,
			Timestamp: int64(i + 1),
		}); err != nil {
			b.Fatalf("insert bid: %v", err)
		}
		if _, err := ob.OnOrderUpdate(&OrderUpdate{
			Type:      OrderUpdateInsert,
			OrderID:   int64(levels + i + 1),
			Price:     int64(100001 + i),
			Quantity:  100,
			Side:      SideSell,
			Timestamp: int64(levels + i + 1),
		}); err != nil {
			b.Fatalf("insert ask: %v", err)
		}
	}
	return ob
}

func BenchmarkHFTOrderBookOnOrderUpdateInsert(b *testing.B) {
	ob := NewHFTOrderBook("BENCH", 1, 0.01)
	defer ob.Close()

	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := ob.OnOrderUpdate(&OrderUpdate{
			Type:      OrderUpdateInsert,
			OrderID:   int64(i + 1),
			Price:     100000,
			Quantity:  100,
			Side:      SideBuy,
			Timestamp: int64(i + 1),
		})
		if err != nil {
			b.Fatalf("insert update: %v", err)
		}
	}
}

func BenchmarkHFTOrderBookOnOrderUpdateModify(b *testing.B) {
	ob := seedBenchmarkHFTOrderBook(b, 1000)
	defer ob.Close()

	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := ob.OnOrderUpdate(&OrderUpdate{
			Type:      OrderUpdateModify,
			OrderID:   1,
			Quantity:  int64(100 + (i & 1)),
			Timestamp: int64(i + 1),
		})
		if err != nil {
			b.Fatalf("modify update: %v", err)
		}
	}
}

func BenchmarkHFTOrderBookGetCachedMetrics(b *testing.B) {
	ob := seedBenchmarkHFTOrderBook(b, 1000)
	defer ob.Close()

	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if ob.GetImbalance() == nil || ob.GetLiquidity() == nil || ob.GetSignal() == nil {
			b.Fatal("missing cached metrics")
		}
	}
}
