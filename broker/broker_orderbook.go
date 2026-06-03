// Package broker provides broker-specific order book management functionality.
package broker

// This file implements a read-only order book reconstruction layer for brokerage use cases.
// It receives incremental updates and snapshots from exchanges and maintains a local order book state
// without performing order matching.

import (
	"errors"
	"fmt"
	"sync/atomic"
	"time"

	"github.com/finosorg-labs/exchange"
	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

var (
	ErrNilOrderIncrement = errors.New("nil order increment")
	ErrNilTrade          = errors.New("nil trade")
	ErrNilSnapshot       = errors.New("nil exchange snapshot")
	ErrNilSubscriber     = errors.New("nil snapshot subscriber")
)

// Side represents order side (buy or sell).
type Side = coreob.Side

const (
	SideBuy  = coreob.SideBuy
	SideSell = coreob.SideSell
)

// Order represents a single order in the order book.
type Order = coreob.Order

// PriceLevel represents aggregated orders at a specific price level.
type PriceLevel = coreob.PriceLevel

// Snapshot represents a complete order book snapshot with top N levels.
type Snapshot = exchange.OrderBookSnapshot

// OrderIncrementType specifies the type of order update.
type OrderIncrementType int

const (
	OrderInsert OrderIncrementType = iota // Insert a new order
	OrderDelete                  // Delete an existing order
	OrderModify                   // Modify an existing order's quantity
)

// OrderIncrement represents an incremental order book update from the exchange.
type OrderIncrement struct {
	Type      OrderIncrementType
	OrderID   int64
	Price     int64 // Required for OrderInsert
	Quantity  int64 // Required for OrderInsert and OrderModify
	Side      Side  // Required for OrderInsert
	Timestamp int64
}

// Trade represents a matched trade that affects order book state.
// Supports both partial fills and complete fills.
type Trade struct {
	BuyOrderID  int64 // Order ID of the buy side, 0 if not applicable
	SellOrderID int64 // Order ID of the sell side, 0 if not applicable
	Price     int64 // Trade execution price
	Quantity    int64 // Trade quantity (may be less than order quantity for partial fills)
	Timestamp   int64
}

// ExchangeSnapshot represents a complete order book snapshot from the exchange.
// Contains aggregated price levels rather than individual orders.
type ExchangeSnapshot struct {
	SymbolID  uint32
	Bids      []PriceLevel // Bid price levels (descending order)
	Asks      []PriceLevel // Ask price levels (ascending order)
	Timestamp time.Time
}

// SnapshotSubscriber receives order book snapshot updates.
type SnapshotSubscriber interface {
	OnSnapshot(snapshot *Snapshot) error
}

// BrokerMetrics tracks order book update and snapshot generation metrics.
// All methods are thread-safe using atomic operations.
type BrokerMetrics struct {
	updateCount     atomic.Int64 // Total number of order book updates
	snapshotCount   atomic.Int64 // Total number of snapshots generated
	lastUpdateNanos atomic.Int64 // Timestamp of last update in nanoseconds
}

// UpdateCount returns the total number of order book updates processed.
func (m *BrokerMetrics) UpdateCount() int64 {
	return m.updateCount.Load()
}

// SnapshotCount returns the total number of snapshots generated.
func (m *BrokerMetrics) SnapshotCount() int64 {
	return m.snapshotCount.Load()
}

// LastUpdate returns the timestamp of the last order book update.
// Returns zero time if no updates have been processed.
func (m *BrokerMetrics) LastUpdate() time.Time {
	nanos := m.lastUpdateNanos.Load()
	if nanos == 0 {
		return time.Time{}
	}
	return time.Unix(0, nanos)
}

// SnapshotPublisher manages a list of snapshot subscribers and publishes updates to them.
// Publishing uses fail-fast semantics: stops on first error.
type SnapshotPublisher struct {
	subscribers []SnapshotSubscriber
}

// Subscribe adds a new snapshot subscriber.
// Returns ErrNilSubscriber if subscriber is nil.
func (p *SnapshotPublisher) Subscribe(subscriber SnapshotSubscriber) error {
	if subscriber == nil {
		return ErrNilSubscriber
	}
	p.subscribers = append(p.subscribers, subscriber)
	return nil
}

// Publish sends a snapshot to all subscribers.
// Stops and returns error on first subscriber error (fail-fast semantics).
func (p *SnapshotPublisher) Publish(snapshot *Snapshot) error {
	for _, subscriber := range p.subscribers {
		if err := subscriber.OnSnapshot(snapshot); err != nil {
			return err
		}
	}
	return nil
}

// BrokerOrderBook implements a read-only order book reconstruction layer for brokerage use cases.
// It receives incremental updates and snapshots from exchanges and maintains a local order book state
// without performing order matching.
//
// Key features:
//   - Incremental update processing (OnOrderIncrement)
//   - Trade event handling with partial fill support (OnTrade)
//   - Snapshot-based synchronization (OnSnapshot)
//   - Multi-level snapshot generation (GetSnapshot)
//   - Publisher-subscriber pattern for snapshot distribution
//
// Concurrency: NOT thread-safe. Caller must serialize access.
type BrokerOrderBook struct {
	core      *coreob.OrderBook
	metrics   *BrokerMetrics
	publisher *SnapshotPublisher
}

// NewBrokerOrderBook creates a new broker order book for the given symbol.
func NewBrokerOrderBook(symbol string, symbolID uint32) *BrokerOrderBook {
	return &BrokerOrderBook{
		core:      coreob.NewOrderBook(symbol, symbolID),
		metrics:   &BrokerMetrics{},
		publisher: &SnapshotPublisher{},
	}
}

// OnOrderIncrement processes an incremental order book update.
// Supports insert, delete, and modify operations.
//
// Returns an error if:
//   - inc is nil
//   - the increment type is invalid
//   - the underlying order book operation fails
func (b *BrokerOrderBook) OnOrderIncrement(inc *OrderIncrement) error {
	if inc == nil {
		return ErrNilOrderIncrement
	}

	var err error
	switch inc.Type {
	case OrderInsert:
		err = b.core.InsertOrder(&Order{
			OrderID:   inc.OrderID,
			Price:     inc.Price,
			Quantity:  inc.Quantity,
		Side:      inc.Side,
			Timestamp: inc.Timestamp,
		})
	case OrderDelete:
		err = b.core.DeleteOrder(inc.OrderID)
	case OrderModify:
		err = b.core.ModifyOrder(inc.OrderID, inc.Quantity)
	default:
		err = fmt.Errorf("invalid order increment type: %d", inc.Type)
	}
	if err != nil {
		return err
	}

	b.metrics.updateCount.Add(1)
	b.metrics.lastUpdateNanos.Store(time.Now().UnixNano())
	return nil
}

// OnTrade processes a trade event and updates affected orders.
// Supports partial fills: reduces order quantity if trade quantity < order quantity.
// Deletes orders that are completely filled.
//
// Trade processing logic:
//   - If trade.Quantity >= order.Quantity: delete the order (complete fill)
//   - If trade.Quantity < order.Quantity: reduce order quantity (partial fill)
//
// Returns an error if:
//   - trade is nil
//   - order lookup fails
//   - order deletion or modification fails
func (b *BrokerOrderBook) OnTrade(trade *Trade) error {
	if trade == nil {
		return ErrNilTrade
	}

	// Process buy side
	if trade.BuyOrderID != 0 && b.core.ContainsOrder(trade.BuyOrderID) {
		buyOrder := b.core.GetOrderDetails(trade.BuyOrderID)
		if buyOrder == nil {
			return fmt.Errorf("buy order %d not found", trade.BuyOrderID)
		}
		if buyOrder.Quantity <= trade.Quantity {
			// Complete fill: delete order
			if err := b.core.DeleteOrder(trade.BuyOrderID); err != nil {
				return err
			}
		} else {
			// Partial fill: reduce quantity
			newQty := buyOrder.Quantity - trade.Quantity
			if err := b.core.ModifyOrder(trade.BuyOrderID, newQty); err != nil {
				return err
			}
		}
	}

	// Process sell side
	if trade.SellOrderID != 0 && b.core.ContainsOrder(trade.SellOrderID) {
		sellOrder := b.core.GetOrderDetails(trade.SellOrderID)
		if sellOrder == nil {
			return fmt.Errorf("sell order %d not found", trade.SellOrderID)
		}
		if sellOrder.Quantity <= trade.Quantity {
		// Complete fill: delete order
			if err := b.core.DeleteOrder(trade.SellOrderID); err != nil {
				return err
			}
		} else {
			// Partial fill: reduce quantity
		newQty := sellOrder.Quantity - trade.Quantity
			if err := b.core.ModifyOrder(trade.SellOrderID, newQty); err != nil {
				return err
			}
		}
	}

	b.metrics.updateCount.Add(1)
	b.metrics.lastUpdateNanos.Store(time.Now().UnixNano())
	return nil
}

// OnSnapshot processes an exchange snapshot and rebuilds the order book.
// This method clears all existing orders and reconstructs from the snapshot.
//
// Snapshot handling:
//   - Exchange snapshots contain aggregated price levels, not individual orders
//   - Each price level is inserted as a single order with a virtual OrderID (negative)
//   - Virtual OrderIDs start at -1 and decrement to avoid conflicts with real order IDs
//
// Use cases:
//   - Initial order book synchronization
//   - Recovery after connection loss
//   - Periodic validation against exchange state
//
// Returns an error if:
//   - snap is nil
//   - SymbolID mismatch
//   - Order insertion fails
func (b *BrokerOrderBook) OnSnapshot(snap *ExchangeSnapshot) error {
	if snap == nil {
		return ErrNilSnapshot
	}
	if snap.SymbolID != b.core.SymbolID() {
		return fmt.Errorf("snapshot symbol id mismatch: got %d, want %d", snap.SymbolID, b.core.SymbolID())
	}

	// Clear existing order book state
	b.core.Clear()

	// Use negative OrderIDs for snapshot price levels to avoid conflicts
	// with real order IDs (which are positive)
	virtualOrderID := int64(-1)

	// Insert bid levels
	for _, bid := range snap.Bids {
		if bid.TotalQty <= 0 {
			continue
		}
		order := &Order{
			OrderID:   virtualOrderID,
			Price:     bid.Price,
			Quantity:  bid.TotalQty,
			Side:      SideBuy,
			Timestamp: snap.Timestamp.UnixNano(),
		}
		if err := b.core.InsertOrder(order); err != nil {
			return fmt.Errorf("failed to insert bid level: %w", err)
		}
		virtualOrderID--
	}

	// Insert ask levels
	for _, ask := range snap.Asks {
		if ask.TotalQty <= 0 {
			continue
		}
		order := &Order{
			OrderID:   virtualOrderID,
			Price:     ask.Price,
			Quantity:  ask.TotalQty,
			Side:      SideSell,
			Timestamp: snap.Timestamp.UnixNano(),
		}
		if err := b.core.InsertOrder(order); err != nil {
			return fmt.Errorf("failed to insert ask level: %w", err)
		}
		virtualOrderID--
	}

	b.metrics.updateCount.Add(1)
	b.metrics.lastUpdateNanos.Store(time.Now().UnixNano())
	return nil
}

// GetSnapshot generates an order book snapshot and publishes it to all subscribers.
// Returns the top N levels for both bid and ask sides.
//
// Note: This method both generates and publishes the snapshot.
// If any subscriber returns an error, snapshot generation fails.
//
// Parameters:
//   - topN: Maximum number of price levels per side (must be non-negative)
//   - precision: Volume aggregation precision mode
//
// Returns an error if:
//   - topN is negative
//   - Snapshot generation fails
//   - Snapshot publication fails (any subscriber error)
func (b *BrokerOrderBook) GetSnapshot(topN int, precision exchange.OrderBookPrecisionMode) (*Snapshot, error) {
	if topN < 0 {
		return nil, fmt.Errorf("topN must be non-negative: %d", topN)
	}

	timestamp := time.Now()
	orders := make([]exchange.Order, 0, topN+topN)
	orders = appendLevels(orders, b.core.SymbolID(), b.core.GetTopNBids(topN), exchange.OrderBookSideBid, timestamp)
	orders = appendLevels(orders, b.core.SymbolID(), b.core.GetTopNAsks(topN), exchange.OrderBookSideAsk, timestamp)

	snapshot, err := exchange.GenerateSnapshot(orders, uint32(topN), precision, timestamp)
	if err != nil {
		return nil, err
	}

	b.metrics.snapshotCount.Add(1)
	if err := b.publisher.Publish(snapshot); err != nil {
		return nil, err
	}
	return snapshot, nil
}

// Subscribe adds a snapshot subscriber.
// The subscriber will receive all future snapshots generated by GetSnapshot.
func (b *BrokerOrderBook) Subscribe(subscriber SnapshotSubscriber) error {
	return b.publisher.Subscribe(subscriber)
}

// Close closes the order book and releases resources.
// Returns an error if the order book is already closed.
func (b *BrokerOrderBook) Close() error {
	return b.core.Close()
}

// appendLevels converts price levels to exchange orders for snapshot generation.
// Helper function used by GetSnapshot.
func appendLevels(
	orders []exchange.Order,
	symbolID uint32,
	levels []*PriceLevel,
	side exchange.OrderBookSide,
	timestamp time.Time,
) []exchange.Order {
	for _, level := range levels {
		if level == nil {
			continue
		}
		orders = append(orders, exchange.Order{
			SymbolID:  symbolID,
			Price:     float64(level.Price),
			Volume:    float64(level.TotalQty),
			Side:      side,
			Timestamp: timestamp,
		})
	}
	return orders
}
