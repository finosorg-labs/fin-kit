// Package broker provides broker-specific order book management functionality.
package broker

// This file implements a read-only order book reconstruction layer for brokerage use cases.
// It encapsulates the core order book and provides broker-specific functionality:
//   - Incremental update processing (OnOrderIncrement)
//   - Trade event handling with partial fill support (OnTrade)
//   - Snapshot-based synchronization (OnSnapshot)

import (
	"fmt"
	"time"

	"github.com/finosorg-labs/exchange"
	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// BrokerOrderBook implements a read-only order book reconstruction layer for brokerage use cases.
// It receives incremental updates and snapshots from exchanges and maintains a local order book state
// without performing order matching.
//
// Key features:
//   - Incremental update processing (OnOrderIncrement)
//   - Trade event handling with partial fill support (OnTrade)
//   - Snapshot-based synchronization (OnSnapshot)
//
// Concurrency: NOT thread-safe. Caller must serialize access.
type BrokerOrderBook struct {
	core     *coreob.OrderBook
	metrics  *BrokerMetrics
	snapshot *SnapshotBuilder
	tickSize float64 // Price unit for int64 to float64 conversion
}

// NewBrokerOrderBook creates a new broker order book for the given symbol.
// tickSize specifies the price unit for converting integer prices to float64.
// For example: tickSize=0.001 means price 10250 represents 10.25.
func NewBrokerOrderBook(symbol string, symbolID uint32, tickSize float64) *BrokerOrderBook {
	if tickSize <= 0 {
		tickSize = 0.001 // Default to 0.001 (minimum price unit for most stocks)
	}
	return &BrokerOrderBook{
		core:     coreob.NewOrderBook(symbol, symbolID),
		metrics:  &BrokerMetrics{},
		snapshot: NewSnapshotBuilder(),
		tickSize: tickSize,
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

	b.metrics.RecordUpdate()
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
//   - order deletion or modification fails
func (b *BrokerOrderBook) OnTrade(trade *Trade) error {
	if trade == nil {
		return ErrNilTrade
	}

	// Process buy side
	if trade.BuyOrderID != 0 {
		buyOrder := b.core.GetOrderDetails(trade.BuyOrderID)
		if buyOrder != nil {
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
		// If buyOrder is nil, the order was already deleted or doesn't exist - this is acceptable
	}

	// Process sell side
	if trade.SellOrderID != 0 {
		sellOrder := b.core.GetOrderDetails(trade.SellOrderID)
		if sellOrder != nil {
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
		// If sellOrder is nil, the order was already deleted or doesn't exist - this is acceptable
	}

	b.metrics.RecordUpdate()
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

	b.metrics.RecordUpdate()
	return nil
}

// GetSnapshot generates a broker order book snapshot.
// This method delegates snapshot building to the SnapshotBuilder and records the operation in metrics.
//
// Parameters:
//   - topN: Maximum number of price levels per side
//   - precision: Volume aggregation precision mode
//
// Returns an error if:
//   - topN is negative
//   - Snapshot generation fails
//   - Snapshot publication fails (any subscriber error)
func (b *BrokerOrderBook) GetSnapshot(topN int, precision exchange.OrderBookPrecisionMode) (*Snapshot, error) {
	snapshot, err := b.snapshot.BuildAndPublish(
		b.core.SymbolID(),
		b.core.GetTopNBids(topN),
		b.core.GetTopNAsks(topN),
		topN,
		precision,
		time.Now(),
		b.tickSize, // Pass tickSize for proper price conversion
	)
	if err != nil {
		return nil, err
	}
	b.metrics.RecordSnapshot()
	return snapshot, nil
}

// Subscribe adds a snapshot subscriber.
// The subscriber will receive all future snapshots generated by GetSnapshot.
func (b *BrokerOrderBook) Subscribe(subscriber SnapshotSubscriber) error {
	return b.snapshot.Subscribe(subscriber)
}

// Close closes the order book and releases resources.
// Returns an error if the order book is already closed.
func (b *BrokerOrderBook) Close() error {
	return b.core.Close()
}

// Metrics returns the broker metrics for this order book.
func (b *BrokerOrderBook) Metrics() *BrokerMetrics {
	return b.metrics
}

// SymbolID returns the symbol ID of this order book.
func (b *BrokerOrderBook) SymbolID() uint32 {
	return b.core.SymbolID()
}

// Symbol returns the symbol name of this order book.
func (b *BrokerOrderBook) Symbol() string {
	return b.core.Symbol()
}
