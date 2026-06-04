// Package exchange implements a matching engine for exchange order book management.
package exchange

// This file defines order types, trade structures, and common errors.

import (
	"errors"
	"fmt"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// Common errors
var (
	ErrNilOrder          = errors.New("nil order")
	ErrNilTrade          = errors.New("nil trade")
	ErrInvalidOrderID    = errors.New("invalid order ID")
	ErrInvalidAccountID  = errors.New("invalid account ID")
	ErrInvalidQuantity   = errors.New("invalid quantity")
	ErrInvalidPrice      = errors.New("invalid price")
	ErrOrderNotFound     = errors.New("order not found")
	ErrSelfTrade         = errors.New("self-trade detected")
	ErrInsufficientQty   = errors.New("insufficient quantity for FOK order")
	ErrInvalidOrderType  = errors.New("invalid order type")
	ErrOrderBookClosed   = errors.New("order book closed")
	ErrDuplicateOrderID  = errors.New("duplicate order ID")
)

// Side represents order side (buy or sell).
type Side = coreob.Side

const (
	SideBuy  = coreob.SideBuy
	SideSell = coreob.SideSell
)

// OrderType represents different order types supported by the matching engine.
type OrderType int

const (
	OrderTypeLimit   OrderType = iota // Limit order: match at specified price or better
	OrderTypeMarket              // Market order: match at best available price
	OrderTypeFOK                   // Fill-Or-Kill: complete fill or cancel
	OrderTypeIOC                  // Immediate-Or-Cancel: fill available, cancel rest
	OrderTypeIceberg                  // Iceberg: display only partial quantity
)

func (t OrderType) String() string {
	switch t {
	case OrderTypeLimit:
		return "Limit"
	case OrderTypeMarket:
		return "Market"
	case OrderTypeFOK:
		return "FOK"
	case OrderTypeIOC:
		return "IOC"
	case OrderTypeIceberg:
		return "Iceberg"
	default:
		return fmt.Sprintf("Unknown(%d)", t)
	}
}

// Order represents a user order submitted to the matching engine.
type Order struct {
	OrderID       int64     // Unique order identifier
	AccountID     string    // Account identifier for self-trade prevention
	Symbol        string    // Trading symbol (e.g., "BTCUSD")
	Price         int64     // Order price (integer representation)
	Quantity      int64     // Total order quantity
	VisibleQty    int64     // Visible quantity for iceberg orders (0 = show all)
	Side          Side      // Buy or Sell
	Type          OrderType // Order type
	Timestamp     int64     // Order submission timestamp (nanoseconds)

	// Internal fields for tracking
	FilledQty     int64     // Quantity already filled
	CanceledQty   int64     // Quantity canceled
	RemainingQty  int64     // Remaining quantity to be filled
}

// NewOrder creates a new order with validation.
func NewOrder(orderID int64, accountID string, price, quantity int64, side Side, orderType OrderType) (*Order, error) {
	if orderID <= 0 {
		return nil, ErrInvalidOrderID
	}
	if accountID == "" {
		return nil, ErrInvalidAccountID
	}
	if quantity <= 0 {
		return nil, ErrInvalidQuantity
	}
	if orderType == OrderTypeLimit && price <= 0 {
		return nil, ErrInvalidPrice
	}

	return &Order{
		OrderID:      orderID,
		AccountID:    accountID,
		Price:        price,
		Quantity:     quantity,
		Side:         side,
		Type:         orderType,
		RemainingQty: quantity,
	}, nil
}

// NewIcebergOrder creates a new iceberg order with visible quantity.
func NewIcebergOrder(orderID int64, accountID string, price, quantity, visibleQty int64, side Side) (*Order, error) {
	if visibleQty <= 0 || visibleQty > quantity {
		return nil, fmt.Errorf("invalid visible quantity: %d (total: %d)", visibleQty, quantity)
	}

	order, err := NewOrder(orderID, accountID, price, quantity, side, OrderTypeIceberg)
	if err != nil {
		return nil, err
	}
	order.VisibleQty = visibleQty
	return order, nil
}

// Trade represents a matched trade between two orders.
type Trade struct {
	TradeID      int64  // Unique trade identifier
	BuyOrderID   int64  // Buy side order ID
	SellOrderID  int64  // Sell side order ID
	BuyAccountID string // Buy side account ID
	SellAccountID string // Sell side account ID
	Price      int64  // Execution price
	Quantity     int64  // Execution quantity
	Timestamp    int64  // Execution timestamp (nanoseconds)

	// Fill information
	BuyFillType  FillType // Buy order fill type
	SellFillType FillType // Sell order fill type
}

// FillType represents the fill status of an order after a trade.
type FillType int

const (
	FillTypePartial  FillType = iota // Partial fill: order still has remaining quantity
	FillTypeComplete                 // Complete fill: order fully executed
	FillTypeCanceled               // Order canceled (IOC, FOK)
)

func (f FillType) String() string {
	switch f {
	case FillTypePartial:
		return "Partial"
	case FillTypeComplete:
		return "Complete"
	case FillTypeCanceled:
		return "Canceled"
	default:
		return fmt.Sprintf("Unknown(%d)", f)
	}
}

// OrderStatus represents the current status of an order.
type OrderStatus int

const (
	OrderStatusNew       OrderStatus = iota // Order submitted, not yet matched
	OrderStatusPartial                      // Order partially filled
	OrderStatusFilled              // Order completely filled
	OrderStatusCanceled                     // Order canceled
	OrderStatusRejected           // Order rejected
)

func (s OrderStatus) String() string {
	switch s {
	case OrderStatusNew:
		return "New"
	case OrderStatusPartial:
		return "Partial"
	case OrderStatusFilled:
		return "Filled"
	case OrderStatusCanceled:
		return "Canceled"
	case OrderStatusRejected:
		return "Rejected"
	default:
		return fmt.Sprintf("Unknown(%d)", s)
	}
}

// ExecutionReport contains details about an order's execution.
type ExecutionReport struct {
	OrderID       int64    // Order ID
	AccountID     string      // Account ID
	Status        OrderStatus // Order status after execution
	FilledQty     int64       // Total quantity filled
	RemainingQty  int64     // Remaining quantity
	AvgPrice      float64     // Average fill price
	Trades        []*Trade    // Trades generated by this order
	RejectReason  string      // Rejection reason (if rejected)
	Timestamp     int64     // Report timestamp
}
