package broker

import (
	"encoding/json"
	"testing"
	"time"
)

func TestMarketDataAdapterParseOrderIncrement(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	// Test insert order
	insertData := map[string]interface{}{
		"type":      "insert",
		"order_id":  12345,
		"price":     10000,
		"quantity":  100,
		"side":      "buy",
		"timestamp": 1234567890,
	}
	jsonData, _ := json.Marshal(insertData)

	raw := &RawMarketData{
		Type:      TypeOrderIncrement,
		Format:    FormatJSON,
		Data:      jsonData,
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	inc, err := adapter.ParseOrderIncrement(raw)
	if err != nil {
		t.Fatalf("ParseOrderIncrement failed: %v", err)
	}
	if inc.Type != OrderInsert {
		t.Errorf("Expected OrderInsert, got %v", inc.Type)
	}
	if inc.OrderID != 12345 {
		t.Errorf("Expected OrderID=12345, got %d", inc.OrderID)
	}
	if inc.Price != 10000 {
		t.Errorf("Expected Price=10000, got %d", inc.Price)
	}
	if inc.Quantity != 100 {
		t.Errorf("Expected Quantity=100, got %d", inc.Quantity)
	}
	if inc.Side != SideBuy {
		t.Errorf("Expected SideBuy, got %v", inc.Side)
	}
}

func TestMarketDataAdapterParseOrderIncrementDelete(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	deleteData := map[string]interface{}{
		"type":      "delete",
		"order_id":  12345,
		"price":     0,
		"quantity":  0,
		"side":      "sell",
		"timestamp": 1234567890,
	}
	jsonData, _ := json.Marshal(deleteData)

	raw := &RawMarketData{
		Type:      TypeOrderIncrement,
		Format:    FormatJSON,
		Data:      jsonData,
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	inc, err := adapter.ParseOrderIncrement(raw)
	if err != nil {
		t.Fatalf("ParseOrderIncrement failed: %v", err)
	}
	if inc.Type != OrderDelete {
		t.Errorf("Expected OrderDelete, got %v", inc.Type)
	}
	if inc.Side != SideSell {
		t.Errorf("Expected SideSell, got %v", inc.Side)
	}
}

func TestMarketDataAdapterParseOrderIncrementModify(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	modifyData := map[string]interface{}{
		"type":      "modify",
		"order_id":  12345,
		"price":     10000,
		"quantity":  50,
		"side":      "buy",
		"timestamp": 1234567890,
	}
	jsonData, _ := json.Marshal(modifyData)

	raw := &RawMarketData{
		Type:      TypeOrderIncrement,
		Format:    FormatJSON,
		Data:      jsonData,
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	inc, err := adapter.ParseOrderIncrement(raw)
	if err != nil {
		t.Fatalf("ParseOrderIncrement failed: %v", err)
	}
	if inc.Type != OrderModify {
		t.Errorf("Expected OrderModify, got %v", inc.Type)
	}
}

func TestMarketDataAdapterParseOrderIncrementErrors(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)
	// Test nil raw data
	_, err := adapter.ParseOrderIncrement(nil)
	if err == nil {
		t.Error("Expected error for nil raw data")
	}

	// Test wrong message type
	raw := &RawMarketData{
		Type:      TypeTrade,
		Format:    FormatJSON,
		Data:      []byte("{}"),
		Timestamp: time.Now(),
		SymbolID:  1,
	}
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for wrong message type")
	}

	// Test invalid JSON
	raw.Type = TypeOrderIncrement
	raw.Data = []byte("{invalid json}")
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for invalid JSON")
	}

	// Test invalid order type
	invalidData := map[string]interface{}{
		"type":    "invalid_type",
		"order_id":  12345,
		"price":     10000,
		"quantity":  100,
		"side":      "buy",
		"timestamp": 1234567890,
	}
	jsonData, _ := json.Marshal(invalidData)
	raw.Data = jsonData
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for invalid order type")
	}

	// Test invalid side
	invalidSideData := map[string]interface{}{
		"type":      "insert",
		"order_id":  12345,
		"price":     10000,
		"quantity":  100,
		"side":      "invalid_side",
		"timestamp": 1234567890,
	}
	jsonData, _ = json.Marshal(invalidSideData)
	raw.Data = jsonData
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for invalid side")
	}

	// Test binary format (not implemented)
	raw.Format = FormatBinary
	raw.Data = []byte("{}")
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for binary format")
	}
}

func TestMarketDataAdapterParseTrade(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	tradeData := map[string]interface{}{
		"buy_order_id":  12345,
		"sell_order_id": 67890,
		"price":         10000,
		"quantity":      50,
		"timestamp":     1234567890,
	}
	jsonData, _ := json.Marshal(tradeData)

	raw := &RawMarketData{
		Type:      TypeTrade,
		Format:    FormatJSON,
		Data:      jsonData,
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	trade, err := adapter.ParseTrade(raw)
	if err != nil {
		t.Fatalf("ParseTrade failed: %v", err)
	}
	if trade.BuyOrderID != 12345 {
		t.Errorf("Expected BuyOrderID=12345, got %d", trade.BuyOrderID)
	}
	if trade.SellOrderID != 67890 {
		t.Errorf("Expected SellOrderID=67890, got %d", trade.SellOrderID)
	}
	if trade.Price != 10000 {
		t.Errorf("Expected Price=10000, got %d", trade.Price)
	}
	if trade.Quantity != 50 {
		t.Errorf("Expected Quantity=50, got %d", trade.Quantity)
	}
}

func TestMarketDataAdapterParseTradeErrors(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	// Test nil raw data
	_, err := adapter.ParseTrade(nil)
	if err == nil {
		t.Error("Expected error for nil raw data")
	}

	// Test wrong message type
	raw := &RawMarketData{
		Type:    TypeOrderIncrement,
		Format:    FormatJSON,
		Data:      []byte("{}"),
		Timestamp: time.Now(),
		SymbolID:  1,
	}
	_, err = adapter.ParseTrade(raw)
	if err == nil {
		t.Error("Expected error for wrong message type")
	}

	// Test invalid JSON
	raw.Type = TypeTrade
	raw.Data = []byte("{invalid json}")
	_, err = adapter.ParseTrade(raw)
	if err == nil {
		t.Error("Expected error for invalid JSON")
	}
}

func TestMarketDataAdapterParseSnapshot(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	snapshotData := map[string]interface{}{
		"symbol_id": 1,
		"bids": []map[string]interface{}{
			{"price": 100, "quantity": 10},
			{"price": 99, "quantity": 20},
		},
		"asks": []map[string]interface{}{
			{"price": 101, "quantity": 15},
		{"price": 102, "quantity": 25},
		},
		"timestamp": 1234567890,
	}
	jsonData, _ := json.Marshal(snapshotData)

	raw := &RawMarketData{
		Type:      TypeSnapshot,
		Format:    FormatJSON,
		Data:      jsonData,
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	snap, err := adapter.ParseSnapshot(raw)
	if err != nil {
		t.Fatalf("ParseSnapshot failed: %v", err)
	}
	if snap.SymbolID != 1 {
		t.Errorf("Expected SymbolID=1, got %d", snap.SymbolID)
	}
	if len(snap.Bids) != 2 {
		t.Errorf("Expected 2 bids, got %d", len(snap.Bids))
	}
	if len(snap.Asks) != 2 {
		t.Errorf("Expected 2 asks, got %d", len(snap.Asks))
	}
	if snap.Bids[0].Price != 100 || snap.Bids[0].TotalQty != 10 {
		t.Errorf("Bid[0] incorrect: price=%d, qty=%d", snap.Bids[0].Price, snap.Bids[0].TotalQty)
	}
}

func TestMarketDataAdapterParseSnapshotErrors(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	// Test nil raw data
	_, err := adapter.ParseSnapshot(nil)
	if err == nil {
		t.Error("Expected error for nil raw data")
	}

	// Test wrong message type
	raw := &RawMarketData{
		Type:      TypeTrade,
		Format:    FormatJSON,
		Data:      []byte("{}"),
		Timestamp: time.Now(),
		SymbolID:  1,
	}
	_, err = adapter.ParseSnapshot(raw)
	if err == nil {
		t.Error("Expected error for wrong message type")
	}

	// Test invalid JSON
	raw.Type = TypeSnapshot
	raw.Data = []byte("{invalid json}")
	_, err = adapter.ParseSnapshot(raw)
	if err == nil {
		t.Error("Expected error for invalid JSON")
	}
}

func TestMarketDataAdapterValidateRawData(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	// Test valid data
	raw := &RawMarketData{
		Type:      TypeOrderIncrement,
		Format:    FormatJSON,
		Data:      []byte("{}"),
		Timestamp: time.Now(),
		SymbolID:  1,
	}
	if err := adapter.ValidateRawData(raw); err != nil {
		t.Errorf("Expected valid data to pass, got error: %v", err)
	}

	// Test nil data
	if err := adapter.ValidateRawData(nil); err == nil {
		t.Error("Expected error for nil data")
	}

	// Test empty data
	raw.Data = []byte{}
	if err := adapter.ValidateRawData(raw); err == nil {
		t.Error("Expected error for empty data")
	}
	raw.Data = []byte("{}")

	// Test invalid symbol ID
	raw.SymbolID = 0
	if err := adapter.ValidateRawData(raw); err == nil {
	t.Error("Expected error for invalid symbol ID")
	}
	raw.SymbolID = 1

	// Test invalid timestamp
	raw.Timestamp = time.Time{}
	if err := adapter.ValidateRawData(raw); err == nil {
		t.Error("Expected error for invalid timestamp")
	}
}

func TestMarketDataAdapterUnsupportedFormats(t *testing.T) {
	adapter := NewMarketDataAdapter(FormatJSON)

	raw := &RawMarketData{
		Type:      TypeOrderIncrement,
		Format:    FormatFIX,
		Data:      []byte("{}"),
		Timestamp: time.Now(),
		SymbolID:  1,
	}

	// Test FIX format for ParseOrderIncrement
	_, err := adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for FIX format in ParseOrderIncrement")
	}

	// Test FIX format for ParseTrade
	raw.Type = TypeTrade
	_, err = adapter.ParseTrade(raw)
	if err == nil {
		t.Error("Expected error for FIX format in ParseTrade")
	}

	// Test FIX format for ParseSnapshot
	raw.Type = TypeSnapshot
	_, err = adapter.ParseSnapshot(raw)
	if err == nil {
		t.Error("Expected error for FIX format in ParseSnapshot")
	}

	// Test unsupported format
	raw.Format = MarketDataFormat(999)
	raw.Type = TypeOrderIncrement
	_, err = adapter.ParseOrderIncrement(raw)
	if err == nil {
		t.Error("Expected error for unsupported format")
	}
}
