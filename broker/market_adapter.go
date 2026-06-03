// Package broker provides broker-specific order book management functionality.
package broker

// This file implements market data adapter functionality.
// It parses raw market data from exchanges and converts it into standardized formats
// for order book reconstruction.

import (
	"encoding/json"
	"fmt"
	"time"
)

// MarketDataAdapter adapts raw market data from exchanges into standardized formats.
// It handles format parsing, validation, and conversion for order book reconstruction.
type MarketDataAdapter struct {
	format MarketDataFormat // Expected data format
}

// NewMarketDataAdapter creates a new market data adapter for the specified format.
func NewMarketDataAdapter(format MarketDataFormat) *MarketDataAdapter {
	return &MarketDataAdapter{
		format: format,
	}
}

// ParseOrderIncrement parses raw market data into an OrderIncrement.
// Supports JSON format by default, with extensibility for other formats.
func (a *MarketDataAdapter) ParseOrderIncrement(raw *RawMarketData) (*OrderIncrement, error) {
	if raw == nil {
		return nil, fmt.Errorf("raw market data is nil")
	}
	if raw.Type != TypeOrderIncrement {
		return nil, fmt.Errorf("invalid message type: expected OrderIncrement, got %d", raw.Type)
	}

	switch raw.Format {
	case FormatJSON:
		return a.parseOrderIncrementJSON(raw)
	case FormatBinary:
		return nil, fmt.Errorf("binary format not yet implemented")
	case FormatFIX:
		return nil, fmt.Errorf("FIX format not yet implemented")
	default:
		return nil, fmt.Errorf("unsupported format: %d", raw.Format)
	}
}

// ParseTrade parses raw market data into a Trade.
func (a *MarketDataAdapter) ParseTrade(raw *RawMarketData) (*Trade, error) {
	if raw == nil {
		return nil, fmt.Errorf("raw market data is nil")
	}
	if raw.Type != TypeTrade {
		return nil, fmt.Errorf("invalid message type: expected Trade, got %d", raw.Type)
	}

	switch raw.Format {
	case FormatJSON:
		return a.parseTradeJSON(raw)
	case FormatBinary:
		return nil, fmt.Errorf("binary format not yet implemented")
	case FormatFIX:
		return nil, fmt.Errorf("FIX format not yet implemented")
	default:
		return nil, fmt.Errorf("unsupported format: %d", raw.Format)
	}
}

// ParseSnapshot parses raw market data into an ExchangeSnapshot.
func (a *MarketDataAdapter) ParseSnapshot(raw *RawMarketData) (*ExchangeSnapshot, error) {
	if raw == nil {
		return nil, fmt.Errorf("raw market data is nil")
	}
	if raw.Type != TypeSnapshot {
		return nil, fmt.Errorf("invalid message type: expected Snapshot, got %d", raw.Type)
	}

	switch raw.Format {
	case FormatJSON:
		return a.parseSnapshotJSON(raw)
	case FormatBinary:
		return nil, fmt.Errorf("binary format not yet implemented")
	case FormatFIX:
		return nil, fmt.Errorf("FIX format not yet implemented")
	default:
		return nil, fmt.Errorf("unsupported format: %d", raw.Format)
	}
}

// parseOrderIncrementJSON parses JSON-formatted order increment data.
func (a *MarketDataAdapter) parseOrderIncrementJSON(raw *RawMarketData) (*OrderIncrement, error) {
	var jsonData struct {
		Type      string `json:"type"` // "insert", "delete", "modify"
		OrderID   int64  `json:"order_id"`
		Price     int64  `json:"price"`
		Quantity  int64  `json:"quantity"`
		Side      string `json:"side"` // "buy", "sell"
		Timestamp int64  `json:"timestamp"`
	}

	if err := json.Unmarshal(raw.Data, &jsonData); err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON: %w", err)
	}

	inc := &OrderIncrement{
		OrderID:   jsonData.OrderID,
		Price:     jsonData.Price,
		Quantity:  jsonData.Quantity,
		Timestamp: jsonData.Timestamp,
	}

	// Parse type
	switch jsonData.Type {
	case "insert":
		inc.Type = OrderInsert
	case "delete":
		inc.Type = OrderDelete
	case "modify":
		inc.Type = OrderModify
	default:
		return nil, fmt.Errorf("invalid order type: %s", jsonData.Type)
	}

	// Parse side
	switch jsonData.Side {
	case "buy":
		inc.Side = SideBuy
	case "sell":
		inc.Side = SideSell
	default:
		return nil, fmt.Errorf("invalid side: %s", jsonData.Side)
	}

	return inc, nil
}

// parseTradeJSON parses JSON-formatted trade data.
func (a *MarketDataAdapter) parseTradeJSON(raw *RawMarketData) (*Trade, error) {
	var jsonData struct {
		BuyOrderID  int64 `json:"buy_order_id"`
		SellOrderID int64 `json:"sell_order_id"`
		Price       int64 `json:"price"`
		Quantity    int64 `json:"quantity"`
		Timestamp   int64 `json:"timestamp"`
	}

	if err := json.Unmarshal(raw.Data, &jsonData); err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON: %w", err)
	}

	return &Trade{
		BuyOrderID:  jsonData.BuyOrderID,
		SellOrderID: jsonData.SellOrderID,
		Price:       jsonData.Price,
		Quantity:    jsonData.Quantity,
		Timestamp:   jsonData.Timestamp,
	}, nil
}

// parseSnapshotJSON parses JSON-formatted snapshot data.
func (a *MarketDataAdapter) parseSnapshotJSON(raw *RawMarketData) (*ExchangeSnapshot, error) {
	var jsonData struct {
		SymbolID uint32 `json:"symbol_id"`
		Bids     []struct {
			Price    int64 `json:"price"`
			Quantity int64 `json:"quantity"`
		} `json:"bids"`
		Asks []struct {
			Price    int64 `json:"price"`
			Quantity int64 `json:"quantity"`
		} `json:"asks"`
		Timestamp int64 `json:"timestamp"`
	}

	if err := json.Unmarshal(raw.Data, &jsonData); err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON: %w", err)
	}

	snap := &ExchangeSnapshot{
		SymbolID:  jsonData.SymbolID,
		Bids:      make([]PriceLevel, 0, len(jsonData.Bids)),
		Asks:      make([]PriceLevel, 0, len(jsonData.Asks)),
		Timestamp: time.Unix(0, jsonData.Timestamp),
	}

	// Convert bids
	for _, bid := range jsonData.Bids {
		snap.Bids = append(snap.Bids, PriceLevel{
			Price:    bid.Price,
			TotalQty: bid.Quantity,
		})
	}

	// Convert asks
	for _, ask := range jsonData.Asks {
		snap.Asks = append(snap.Asks, PriceLevel{
			Price:    ask.Price,
			TotalQty: ask.Quantity,
		})
	}

	return snap, nil
}

// ValidateRawData performs basic validation on raw market data.
// Returns an error if the data is invalid or corrupted.
func (a *MarketDataAdapter) ValidateRawData(raw *RawMarketData) error {
	if raw == nil {
		return fmt.Errorf("raw market data is nil")
	}
	if len(raw.Data) == 0 {
		return fmt.Errorf("raw data is empty")
	}
	if raw.SymbolID == 0 {
		return fmt.Errorf("invalid symbol ID: 0")
	}
	if raw.Timestamp.IsZero() {
		return fmt.Errorf("invalid timestamp")
	}
	return nil
}
