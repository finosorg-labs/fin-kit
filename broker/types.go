// Package broker provides broker-specific order book management functionality.
package broker

// This file defines common types and errors used across the broker package.

import (
	"errors"
	"time"

	"github.com/finosorg-labs/exchange"
	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// Common errors
var (
	ErrNilOrderIncrement = errors.New("nil order increment")
	ErrNilTrade        = errors.New("nil trade")
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

// SnapshotSubscriber receives order book snapshot updates.
type SnapshotSubscriber interface {
	OnSnapshot(snapshot *Snapshot) error
}

// OrderIncrementType specifies the type of order update.
type OrderIncrementType int

const (
	OrderInsert OrderIncrementType = iota // Insert a new order
	OrderDelete                       // Delete an existing order
	OrderModify                         // Modify an existing order's quantity
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
	Price       int64 // Trade execution price
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
