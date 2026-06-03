package broker

import (
	"testing"
	"time"

	"github.com/finosorg-labs/exchange"
)

func TestOrderBookValidatorValidate(t *testing.T) {
	validator := NewOrderBookValidator(0.01) // 1% allowed drift

	// Create a rebuilt snapshot
	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 10},
			{Price: 99, Volume: 20},
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 15},
			{Price: 102, Volume: 25},
		},
		Spread: 1,
	}

	// Create matching exchange snapshot
	exchange := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 10},
			{Price: 99, TotalQty: 20},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 15},
			{Price: 102, TotalQty: 25},
		},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchange)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	if !result.Valid {
		t.Errorf("Expected valid=true, got false. Mismatches: bid=%d, ask=%d", result.BidMismatch, result.AskMismatch)
	}
	if result.BidMismatch != 0 || result.AskMismatch != 0 {
		t.Errorf("Expected no mismatches, got bid=%d, ask=%d", result.BidMismatch, result.AskMismatch)
	}
}

func TestOrderBookValidatorValidateQuantityMismatch(t *testing.T) {
	validator := NewOrderBookValidator(0.01) // 1% allowed drift

	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 10},
	},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 15},
		},
	}

	// Exchange has different quantities (beyond drift tolerance)
	exchange := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 20}, // 100% difference
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 30}, // 100% difference
		},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchange)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	if result.Valid {
		t.Error("Expected valid=false for quantity mismatch")
	}
	if result.BidMismatch == 0 || result.AskMismatch == 0 {
		t.Errorf("Expected mismatches, got bid=%d, ask=%d", result.BidMismatch, result.AskMismatch)
	}
}

func TestOrderBookValidatorValidatePriceMismatch(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 10},
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 15},
		},
	}

	// Exchange has different prices
	exchange := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 99, TotalQty: 10}, // Price mismatch
		},
		Asks: []PriceLevel{
			{Price: 102, TotalQty: 15}, // Price mismatch
		},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchange)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	if result.Valid {
		t.Error("Expected valid=false for price mismatch")
	}
	if result.MaxPriceDrift == 0 {
		t.Error("Expected non-zero MaxPriceDrift")
	}
}

func TestOrderBookValidatorValidateLevelCountMismatch(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 10},
			{Price: 99, Volume: 20},
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 15},
		},
	}

	// Exchange has fewer levels
	exchange := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 10},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 15},
			{Price: 102, TotalQty: 25},
		},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchange)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	if result.Valid {
		t.Error("Expected valid=false for level count mismatch")
	}
}

func TestOrderBookValidatorValidateNilInputs(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	// Test nil rebuilt
	_, err := validator.Validate(nil, &ExchangeSnapshot{})
	if err == nil {
		t.Error("Expected error for nil rebuilt snapshot")
	}

	// Test nil exchange
	_, err = validator.Validate(&Snapshot{}, nil)
	if err == nil {
		t.Error("Expected error for nil exchange snapshot")
	}
}

func TestOrderBookValidatorValidateSymbolMismatch(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	rebuilt := &Snapshot{SymbolID: 1}
	exchange := &ExchangeSnapshot{SymbolID: 2}

	_, err := validator.Validate(rebuilt, exchange)
	if err == nil {
		t.Error("Expected error for symbol mismatch")
	}
}

func TestOrderBookValidatorValidateQuick(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 10},
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 15},
		},
		Spread: 1,
	}
	exchangeSnap := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 10},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 15},
		},
		Timestamp: time.Now(),
	}

	if !validator.ValidateQuick(rebuilt, exchangeSnap) {
		t.Error("ValidateQuick should return true for matching snapshots")
	}

	// Test with nil
	if validator.ValidateQuick(nil, exchangeSnap) {
		t.Error("ValidateQuick should return false for nil rebuilt")
	}
	if validator.ValidateQuick(rebuilt, nil) {
		t.Error("ValidateQuick should return false for nil exchangeSnap")
	}

	// Test with symbol mismatch
	exchangeSnap.SymbolID = 2
	if validator.ValidateQuick(rebuilt, exchangeSnap) {
		t.Error("ValidateQuick should return false for symbol mismatch")
	}
	exchangeSnap.SymbolID = 1

	// Test with level count mismatch
	rebuilt.Bids = append(rebuilt.Bids, exchange.PriceLevel{Price: 99, Volume: 20})
	if validator.ValidateQuick(rebuilt, exchangeSnap) {
		t.Error("ValidateQuick should return false for level count mismatch")
	}
}

func TestOrderBookValidatorWithinDriftTolerance(t *testing.T) {
	validator := NewOrderBookValidator(0.05) // 5% allowed drift

	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 100},
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 100},
		},
	}

	// 3% difference - within tolerance
	exchange := &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 103},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 103},
		},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchange)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	if !result.Valid {
		t.Error("Expected valid=true for quantities within drift tolerance")
	}
}

func TestOrderBookValidatorComplexScenarios(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	// Test empty order books
	rebuilt := &Snapshot{
		SymbolID: 1,
		Bids:     []exchange.PriceLevel{},
		Asks:     []exchange.PriceLevel{},
	}
	exchangeSnap := &ExchangeSnapshot{
		SymbolID:  1,
		Bids:      []PriceLevel{},
		Asks:      []PriceLevel{},
		Timestamp: time.Now(),
	}

	result, err := validator.Validate(rebuilt, exchangeSnap)
	if err != nil {
		t.Fatalf("Validate failed for empty books: %v", err)
	}
	if !result.Valid {
		t.Error("Empty order books should validate as equal")
	}

	// Test with zero quantity tolerance
	rebuilt = &Snapshot{
		SymbolID: 1,
		Bids: []exchange.PriceLevel{
			{Price: 100, Volume: 0}, // Zero volume
		},
		Asks: []exchange.PriceLevel{
			{Price: 101, Volume: 100},
		},
	}
	exchangeSnap = &ExchangeSnapshot{
		SymbolID: 1,
		Bids: []PriceLevel{
			{Price: 100, TotalQty: 0},
		},
		Asks: []PriceLevel{
			{Price: 101, TotalQty: 100},
		},
		Timestamp: time.Now(),
	}

	result, err = validator.Validate(rebuilt, exchangeSnap)
	if err != nil {
		t.Fatalf("Validate failed: %v", err)
	}
	// Zero quantities should match
	if !result.Valid {
		t.Error("Expected valid for matching zero quantities")
	}
}

func TestValidateQuickSpreadCheck(t *testing.T) {
	validator := NewOrderBookValidator(0.01)

	// Test with empty bids/asks
	rebuilt := &Snapshot{
		SymbolID: 1,
	Bids:     []exchange.PriceLevel{},
		Asks:     []exchange.PriceLevel{},
	}
	exchangeSnap := &ExchangeSnapshot{
		SymbolID:  1,
		Bids:      []PriceLevel{},
		Asks:      []PriceLevel{},
		Timestamp: time.Now(),
	}

	if !validator.ValidateQuick(rebuilt, exchangeSnap) {
		t.Error("ValidateQuick should return true for empty books")
	}
}
