// Package hft provides high-frequency trading order book functionality.
package hft

// This file defines common types and errors used across the hft package.

import (
	"errors"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
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

// OrderUpdateType specifies the type of order update.
type OrderUpdateType int

const (
	OrderUpdateInsert OrderUpdateType = iota // Insert a new order
	OrderUpdateDelete                        // Delete an existing order
	OrderUpdateModify                        // Modify an existing order's quantity
)

// OrderUpdate represents an order book update event.
type OrderUpdate struct {
	Type      OrderUpdateType
	OrderID   int64
	Price     int64 // Required for OrderUpdateInsert
	Quantity  int64 // Required for OrderUpdateInsert and OrderUpdateModify
	Side      Side  // Required for OrderUpdateInsert
	Timestamp int64
}

// ImbalanceMetrics contains order book imbalance indicators.
type ImbalanceMetrics struct {
	BidAskImbalance  float64 // Buy/sell imbalance ratio
	VolumeImbalance  float64 // Volume imbalance
	PricePressure    float64 // Price pressure direction
	OrderFlowBalance float64 // Order flow balance
}

// LiquidityMetrics contains liquidity indicators.
type LiquidityMetrics struct {
	BidDepth        int64   // Bid side depth
	AskDepth        int64   // Ask side depth
	Spread          int64   // Spread in price ticks
	SpreadBPS       float64 // Spread in basis points
	EffectiveSpread float64 // Effective spread
	MarketImpact    float64 // Market impact cost
}

// SignalType represents the type of trading signal.
type SignalType int

const (
	SignalHold SignalType = iota // Hold position
	SignalBuy                    // Buy signal
	SignalSell                   // Sell signal
)

// Signal represents a trading signal generated from order book analysis.
type Signal struct {
	Type       SignalType // Buy/Sell/Hold
	Strength   float64    // Signal strength [0, 1]
	Confidence float64    // Confidence level [0, 1]
	Timestamp  int64      // Generation timestamp
}

// RiskMetrics contains risk monitoring indicators.
type RiskMetrics struct {
	Position       int64   // Current position
	Exposure       float64 // Position exposure
	MaxDrawdown    float64 // Maximum drawdown
	VaR            float64 // Value at Risk
	ExpectedProfit float64 // Expected profit
}

// Common errors
var (
	ErrNilOrderUpdate = errors.New("nil order update")
	ErrInvalidSide    = errors.New("invalid order side")
	ErrInvalidPrice   = errors.New("invalid price")
	ErrInvalidQty     = errors.New("invalid quantity")
)
