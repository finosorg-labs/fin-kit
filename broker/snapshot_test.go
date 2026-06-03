package broker

import (
	"errors"
	"testing"
	"time"

	"github.com/finosorg-labs/exchange"
)

func TestSnapshotBuilderBuildSnapshot(t *testing.T) {
	builder := NewSnapshotBuilder()

	bids := []*PriceLevel{
		{Price: 100, TotalQty: 10},
		{Price: 99, TotalQty: 20},
	}
	asks := []*PriceLevel{
		{Price: 101, TotalQty: 15},
		{Price: 102, TotalQty: 25},
	}

	snapshot, err := builder.BuildSnapshot(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err != nil {
		t.Fatalf("BuildSnapshot failed: %v", err)
	}
	if snapshot == nil {
		t.Fatal("Expected non-nil snapshot")
	}
	if snapshot.SymbolID != 1 {
		t.Errorf("Expected SymbolID=1, got %d", snapshot.SymbolID)
	}
}

func TestSnapshotBuilderBuildSnapshotNegativeTopN(t *testing.T) {
	builder := NewSnapshotBuilder()

	bids := []*PriceLevel{{Price: 100, TotalQty: 10}}
	asks := []*PriceLevel{{Price: 101, TotalQty: 15}}
	_, err := builder.BuildSnapshot(1, bids, asks, -1, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err == nil {
		t.Error("Expected error for negative topN")
	}
}

func TestSnapshotBuilderBuildSnapshotEmptyLevels(t *testing.T) {
	builder := NewSnapshotBuilder()

	snapshot, err := builder.BuildSnapshot(1, nil, nil, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err != nil {
		t.Fatalf("BuildSnapshot failed: %v", err)
	}
	if len(snapshot.Bids) != 0 || len(snapshot.Asks) != 0 {
		t.Error("Expected empty bids and asks")
	}
}

func TestSnapshotBuilderBuildAndPublish(t *testing.T) {
	builder := NewSnapshotBuilder()

	sub := &mockSubscriber{}
	if err := builder.Subscribe(sub); err != nil {
		t.Fatalf("Subscribe failed: %v", err)
	}

	bids := []*PriceLevel{{Price: 100, TotalQty: 10}}
	asks := []*PriceLevel{{Price: 101, TotalQty: 15}}

	snapshot, err := builder.BuildAndPublish(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err != nil {
		t.Fatalf("BuildAndPublish failed: %v", err)
	}
	if snapshot == nil {
		t.Fatal("Expected non-nil snapshot")
	}

	if len(sub.snapshots) != 1 {
		t.Errorf("Expected subscriber to receive 1 snapshot, got %d", len(sub.snapshots))
	}
	if sub.snapshots[0] != snapshot {
		t.Error("Subscriber received different snapshot")
	}
}

func TestSnapshotBuilderBuildAndPublishFailure(t *testing.T) {
	builder := NewSnapshotBuilder()

	subError := &mockSubscriber{err: errors.New("publish error")}
	if err := builder.Subscribe(subError); err != nil {
		t.Fatalf("Subscribe failed: %v", err)
	}

	bids := []*PriceLevel{{Price: 100, TotalQty: 10}}
	asks := []*PriceLevel{{Price: 101, TotalQty: 15}}

	_, err := builder.BuildAndPublish(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err == nil {
		t.Error("Expected error from failing subscriber")
	}
}

func TestSnapshotBuilderSubscribe(t *testing.T) {
	builder := NewSnapshotBuilder()

	sub := &mockSubscriber{}
	if err := builder.Subscribe(sub); err != nil {
		t.Fatalf("Subscribe failed: %v", err)
	}

	// Test nil subscriber
	if err := builder.Subscribe(nil); err == nil {
		t.Error("Expected error for nil subscriber")
	}
}

func TestSnapshotBuilderBuildSnapshotWithNilLevels(t *testing.T) {
	builder := NewSnapshotBuilder()

	bids := []*PriceLevel{
		{Price: 100, TotalQty: 10},
		nil, // nil level should be skipped
		{Price: 99, TotalQty: 20},
	}
	asks := []*PriceLevel{
		nil, // nil level should be skipped
		{Price: 101, TotalQty: 15},
	}

	snapshot, err := builder.BuildSnapshot(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0.01)
	if err != nil {
		t.Fatalf("BuildSnapshot failed: %v", err)
	}
	// The nil levels should be filtered out in the appendLevels function
	if snapshot == nil {
		t.Fatal("Expected non-nil snapshot")
	}
}

func TestSnapshotBuilderInvalidTickSize(t *testing.T) {
	builder := NewSnapshotBuilder()

	bids := []*PriceLevel{{Price: 100, TotalQty: 10}}
	asks := []*PriceLevel{{Price: 101, TotalQty: 15}}

	// Test zero tickSize
	_, err := builder.BuildSnapshot(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), 0)
	if err == nil {
	t.Error("Expected error for zero tickSize")
	}

	// Test negative tickSize
	_, err = builder.BuildSnapshot(1, bids, asks, 10, exchange.OrderBookPrecisionKahan, time.Now(), -0.01)
	if err == nil {
		t.Error("Expected error for negative tickSize")
	}
}
