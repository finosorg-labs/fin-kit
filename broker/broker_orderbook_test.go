package broker

import (
	"errors"
	"testing"
	"time"

	"github.com/finosorg-labs/exchange"
)

type recordingSubscriber struct {
	snapshots []*Snapshot
	err       error
}

func (s *recordingSubscriber) OnSnapshot(snapshot *Snapshot) error {
	s.snapshots = append(s.snapshots, snapshot)
	return s.err
}

func TestBrokerOrderBookOrderIncrementAndSnapshot(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   1,
		Price:     100,
		Quantity:  10,
		Side:      SideBuy,
		Timestamp: 1000,
	}); err != nil {
		t.Fatalf("insert bid: %v", err)
	}
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   2,
		Price:     105,
		Quantity:  20,
		Side:      SideSell,
		Timestamp: 1001,
	}); err != nil {
		t.Fatalf("insert ask: %v", err)
	}

	snapshot, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot: %v", err)
	}
	if snapshot.SymbolID != 1 {
		t.Fatalf("symbol id = %d, want 1", snapshot.SymbolID)
	}
	if len(snapshot.Bids) != 1 || len(snapshot.Asks) != 1 {
		t.Fatalf("levels = %d bids, %d asks; want 1 bid, 1 ask", len(snapshot.Bids), len(snapshot.Asks))
	}
	if snapshot.Bids[0].Price != 100 || snapshot.Bids[0].Volume != 10 {
		t.Fatalf("bid = %+v, want price 100 volume 10", snapshot.Bids[0])
	}
	if snapshot.Asks[0].Price != 105 || snapshot.Asks[0].Volume != 20 {
		t.Fatalf("ask = %+v, want price 105 volume 20", snapshot.Asks[0])
	}
	if snapshot.Spread != 5 {
		t.Fatalf("spread = %v, want 5", snapshot.Spread)
	}
	if book.metrics.UpdateCount() != 2 {
		t.Fatalf("update count = %d, want 2", book.metrics.UpdateCount())
	}
	if book.metrics.SnapshotCount() != 1 {
		t.Fatalf("snapshot count = %d, want 1", book.metrics.SnapshotCount())
	}
	if book.metrics.LastUpdate().IsZero() {
		t.Fatal("last update was not recorded")
	}
}

func TestBrokerOrderBookModifyAndTradeDelete(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   1,
		Price:     100,
		Quantity:  10,
		Side:      SideBuy,
		Timestamp: 1000,
	}); err != nil {
		t.Fatalf("insert bid: %v", err)
	}
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderModify,
		OrderID:   1,
		Quantity:  15,
		Timestamp: 1001,
	}); err != nil {
		t.Fatalf("modify bid: %v", err)
	}

	snapshot, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot after modify: %v", err)
	}
	if len(snapshot.Bids) != 1 || snapshot.Bids[0].Volume != 15 {
		t.Fatalf("bid levels after modify = %+v, want one level volume 15", snapshot.Bids)
	}

	if err := book.OnTrade(&Trade{BuyOrderID: 1, Quantity: 15, Timestamp: 1002}); err != nil {
		t.Fatalf("trade delete: %v", err)
	}
	snapshot, err = book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot after trade: %v", err)
	}
	if len(snapshot.Bids) != 0 {
		t.Fatalf("bid levels after trade = %+v, want empty", snapshot.Bids)
	}
}

func TestBrokerOrderBookPartialFill(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	// Insert an order with quantity 100
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   1,
		Price:     100,
		Quantity:  100,
		Side:      SideBuy,
		Timestamp: 1000,
	}); err != nil {
		t.Fatalf("insert bid: %v", err)
	}

	// Partial fill: trade 30 out of 100
	if err := book.OnTrade(&Trade{
		BuyOrderID: 1,
		Price:      100,
		Quantity:   30,
		Timestamp:  1001,
	}); err != nil {
		t.Fatalf("partial fill trade: %v", err)
	}

	// Verify order still exists with reduced quantity
	snapshot, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot after partial fill: %v", err)
	}
	if len(snapshot.Bids) != 1 {
		t.Fatalf("bid levels after partial fill = %d, want 1", len(snapshot.Bids))
	}
	if snapshot.Bids[0].Volume != 70 {
		t.Fatalf("bid volume after partial fill = %v, want 70", snapshot.Bids[0].Volume)
	}

	// Complete fill: trade remaining 70
	if err := book.OnTrade(&Trade{
		BuyOrderID: 1,
		Price:    100,
		Quantity:   70,
		Timestamp:  1002,
	}); err != nil {
		t.Fatalf("complete fill trade: %v", err)
	}

	// Verify order is now deleted
	snapshot, err = book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot after complete fill: %v", err)
	}
	if len(snapshot.Bids) != 0 {
		t.Fatalf("bid levels after complete fill = %+v, want empty", snapshot.Bids)
	}

	// Test overfill (should delete order)
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   2,
		Price:     101,
		Quantity:  50,
		Side:      SideSell,
		Timestamp: 2000,
	}); err != nil {
		t.Fatalf("insert ask: %v", err)
	}

	if err := book.OnTrade(&Trade{
		SellOrderID: 2,
		Price:     101,
		Quantity:    100,
		Timestamp:   2001,
	}); err != nil {
		t.Fatalf("overfill trade: %v", err)
	}

	snapshot, err = book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot after overfill: %v", err)
	}
	if len(snapshot.Asks) != 0 {
		t.Fatalf("ask levels after overfill = %+v, want empty", snapshot.Asks)
	}
}

func TestBrokerOrderBookSubscribePublishesSnapshot(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	subscriber := &recordingSubscriber{}
	if err := book.Subscribe(subscriber); err != nil {
		t.Fatalf("subscribe: %v", err)
	}
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   1,
		Price:     100,
		Quantity:  10,
		Side:      SideBuy,
		Timestamp: 1000,
	}); err != nil {
		t.Fatalf("insert bid: %v", err)
	}

	snapshot, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("get snapshot: %v", err)
	}
	if len(subscriber.snapshots) != 1 {
		t.Fatalf("published snapshots = %d, want 1", len(subscriber.snapshots))
	}
	if subscriber.snapshots[0] != snapshot {
		t.Fatal("subscriber did not receive returned snapshot")
	}
}

func TestBrokerOrderBookSubscriberError(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	subscriberErr := errors.New("subscriber failed")
	if err := book.Subscribe(&recordingSubscriber{err: subscriberErr}); err != nil {
		t.Fatalf("subscribe: %v", err)
	}

	_, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if !errors.Is(err, subscriberErr) {
		t.Fatalf("snapshot error = %v, want subscriber error", err)
	}
}

func TestBrokerOrderBookValidationErrors(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	if !errors.Is(book.OnOrderIncrement(nil), ErrNilOrderIncrement) {
		t.Fatal("nil order increment did not return ErrNilOrderIncrement")
	}
	if !errors.Is(book.OnTrade(nil), ErrNilTrade) {
		t.Fatal("nil trade did not return ErrNilTrade")
	}
	if !errors.Is(book.OnSnapshot(nil), ErrNilSnapshot) {
		t.Fatal("nil snapshot did not return ErrNilSnapshot")
	}
	if !errors.Is(book.Subscribe(nil), ErrNilSubscriber) {
		t.Fatal("nil subscriber did not return ErrNilSubscriber")
	}
	if err := book.OnOrderIncrement(&OrderIncrement{Type: OrderIncrementType(99)}); err == nil {
		t.Fatal("invalid order increment type did not fail")
	}
	if _, err := book.GetSnapshot(-1, exchange.OrderBookPrecisionKahan); err == nil {
		t.Fatal("negative topN did not fail")
	}
	if err := book.OnSnapshot(&ExchangeSnapshot{SymbolID: 2}); err == nil {
		t.Fatal("symbol mismatch did not fail")
	}
}

// TestOnSnapshotMultipleOrderIDZero tests if inserting multiple orders with OrderID=0 causes conflicts
func TestOnSnapshotMultipleOrderIDZero(t *testing.T) {
	book := NewBrokerOrderBook("TEST", 1)
	defer book.Close()

	snap := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 10},
			{Price: 99, TotalQty: 20},
			{Price: 98, TotalQty: 30},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 15},
			{Price: 102, TotalQty: 25},
		},
		Timestamp: time.Now(),
	}

	if err := book.OnSnapshot(snap); err != nil {
		t.Fatalf("OnSnapshot failed: %v", err)
	}

	// Verify the snapshot was applied correctly
	snapshot, err := book.GetSnapshot(10, 1)
	if err != nil {
		t.Fatalf("GetSnapshot failed: %v", err)
	}

	if len(snapshot.Bids) != 3 {
		t.Fatalf("expected 3 bid levels, got %d", len(snapshot.Bids))
	}
	if len(snapshot.Asks) != 2 {
		t.Fatalf("expected 2 ask levels, got %d", len(snapshot.Asks))
	}

	// Verify specific levels
	if snapshot.Bids[0].Price != 100 || snapshot.Bids[0].Volume != 10 {
		t.Fatalf("bid[0] = %+v, want price=100 volume=10", snapshot.Bids[0])
	}
	if snapshot.Asks[0].Price != 101 || snapshot.Asks[0].Volume != 15 {
		t.Fatalf("ask[0] = %+v, want price=101 volume=15", snapshot.Asks[0])
	}
}

// TestOnSnapshotRebuildsOrderBook tests that OnSnapshot clears and rebuilds the order book
func TestOnSnapshotRebuildsOrderBook(t *testing.T) {
	book := NewBrokerOrderBook("TEST", 1)
	defer book.Close()

	// Insert some initial orders
	if err := book.OnOrderIncrement(&OrderIncrement{
		Type:      OrderInsert,
		OrderID:   1,
		Price:     100,
		Quantity:  10,
		Side:      SideBuy,
		Timestamp: 1000,
	}); err != nil {
		t.Fatalf("insert order: %v", err)
	}

	// Apply snapshot - should clear the existing order
	snap := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 200, TotalQty: 50},
		},
		Asks:      []PriceLevel{},
		Timestamp: time.Now(),
	}

	if err := book.OnSnapshot(snap); err != nil {
		t.Fatalf("OnSnapshot failed: %v", err)
	}

	// Verify the old order is gone and new snapshot is applied
	snapshot, err := book.GetSnapshot(10, 1)
	if err != nil {
		t.Fatalf("GetSnapshot failed: %v", err)
	}

	if len(snapshot.Bids) != 1 {
		t.Fatalf("expected 1 bid level after snapshot, got %d", len(snapshot.Bids))
	}
	if snapshot.Bids[0].Price != 200 || snapshot.Bids[0].Volume != 50 {
		t.Fatalf("bid = %+v, want price=200 volume=50", snapshot.Bids[0])
	}
}

func TestBrokerOrderBookHelperMethods(t *testing.T) {
	book := NewBrokerOrderBook("AAPL", 1)
	defer book.Close()

	// Test Metrics()
	metrics := book.Metrics()
	if metrics == nil {
		t.Error("Metrics() returned nil")
	}

	// Test SymbolID()
	if book.SymbolID() != 1 {
		t.Errorf("Expected SymbolID=1, got %d", book.SymbolID())
	}

	// Test Symbol()
	if book.Symbol() != "AAPL" {
		t.Errorf("Expected Symbol=AAPL, got %s", book.Symbol())
	}
}

func TestBrokerOrderBookOnTradeNilOrder(t *testing.T) {
	book := NewBrokerOrderBook("TEST", 1)
	defer book.Close()

	// Trade with non-existent order (should not error)
	trade := &Trade{
		BuyOrderID:  99999, // Non-existent order
		SellOrderID: 0,
		Price:       100,
		Quantity:    10,
		Timestamp:   1000,
	}

	if err := book.OnTrade(trade); err != nil {
		t.Fatalf("OnTrade should not fail for non-existent order: %v", err)
	}
}

func TestBrokerOrderBookOnSnapshotEmptyLevels(t *testing.T) {
	book := NewBrokerOrderBook("TEST", 1)
	defer book.Close()
	snap := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 0}, // Zero quantity - should be skipped
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 0}, // Zero quantity - should be skipped
		},
		Timestamp: time.Now(),
	}

	if err := book.OnSnapshot(snap); err != nil {
		t.Fatalf("OnSnapshot failed: %v", err)
	}

	snapshot, err := book.GetSnapshot(10, exchange.OrderBookPrecisionKahan)
	if err != nil {
		t.Fatalf("GetSnapshot failed: %v", err)
	}
	if len(snapshot.Bids) != 0 || len(snapshot.Asks) != 0 {
		t.Error("Expected empty order book after snapshot with zero quantities")
	}
}
