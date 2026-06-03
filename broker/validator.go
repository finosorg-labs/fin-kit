// Package broker provides broker-specific order book management functionality.
package broker

// This file implements order book validation functionality.
// It compares reconstructed order book state against exchange snapshots to detect discrepancies.

import (
	"fmt"
	"math"

	"github.com/finosorg-labs/exchange"
)

// OrderBookValidator validates reconstructed order book against exchange snapshots.
// It detects discrepancies in price levels and quantities to ensure reconstruction accuracy.
type OrderBookValidator struct {
	maxAllowedDrift float64 // Maximum allowed percentage drift (e.g., 0.0001 for 0.01%)
}

// NewOrderBookValidator creates a new order book validator.
// maxAllowedDrift specifies the maximum allowed percentage difference (e.g., 0.0001 for 0.01%).
func NewOrderBookValidator(maxAllowedDrift float64) *OrderBookValidator {
	return &OrderBookValidator{
		maxAllowedDrift: maxAllowedDrift,
	}
}

// Validate compares a rebuilt snapshot against an exchange snapshot.
// Returns a ValidationResult describing any discrepancies found.
//
// Validation checks:
//   - Number of price levels matches
//   - Price at each level matches
//   - Quantity at each level within allowed drift
func (v *OrderBookValidator) Validate(rebuilt *Snapshot, exchange *ExchangeSnapshot) (*ValidationResult, error) {
	if rebuilt == nil {
		return nil, fmt.Errorf("rebuilt snapshot is nil")
	}
	if exchange == nil {
		return nil, fmt.Errorf("exchange snapshot is nil")
	}
	if rebuilt.SymbolID != exchange.SymbolID {
		return nil, fmt.Errorf("symbol mismatch: rebuilt=%d, exchange=%d", rebuilt.SymbolID, exchange.SymbolID)
	}

	result := &ValidationResult{
		Valid:            true,
		MismatchedLevels: make([]LevelMismatch, 0),
	}

	// Validate bid levels
	v.validateSide(rebuilt.Bids, exchange.Bids, SideBuy, result)

	// Validate ask levels
	v.validateSide(rebuilt.Asks, exchange.Asks, SideSell, result)

	// Overall validation passes if no mismatches and within drift tolerance
	if result.BidMismatch > 0 || result.AskMismatch > 0 {
		result.Valid = false
	}

	return result, nil
}

// validateSide validates one side of the order book (bids or asks).
func (v *OrderBookValidator) validateSide(
	rebuiltLevels []exchange.PriceLevel,
	exchangeLevels []PriceLevel,
	side Side,
	result *ValidationResult,
) {
	maxLevels := len(rebuiltLevels)
	if len(exchangeLevels) > maxLevels {
		maxLevels = len(exchangeLevels)
	}

	for i := 0; i < maxLevels; i++ {
		var rebuiltPrice, rebuiltQty float64
		var exchangePrice, exchangeQty float64
		if i < len(rebuiltLevels) {
			rebuiltPrice = rebuiltLevels[i].Price
			rebuiltQty = rebuiltLevels[i].Volume
		}

		if i < len(exchangeLevels) {
			exchangePrice = float64(exchangeLevels[i].Price)
			exchangeQty = float64(exchangeLevels[i].TotalQty)
		}

		// Check if both levels exist
		if i >= len(rebuiltLevels) || i >= len(exchangeLevels) {
			// Level missing in one snapshot
			mismatch := LevelMismatch{
				Side:         side,
				Price:        exchangePrice,
				RebuiltQty:   rebuiltQty,
				ExchangeQty:  exchangeQty,
				QuantityDiff: math.Abs(rebuiltQty - exchangeQty),
			}
			if exchangeQty > 0 {
				mismatch.PercentageDiff = mismatch.QuantityDiff / exchangeQty
			}
			result.MismatchedLevels = append(result.MismatchedLevels, mismatch)
			if side == SideBuy {
				result.BidMismatch++
				result.TotalBidDiff += mismatch.QuantityDiff
			} else {
				result.AskMismatch++
				result.TotalAskDiff += mismatch.QuantityDiff
			}
			continue
		}

		// Check price match
		if math.Abs(rebuiltPrice-exchangePrice) > 0.01 {
			mismatch := LevelMismatch{
				Side:         side,
				Price:        exchangePrice,
				RebuiltQty:   rebuiltQty,
				ExchangeQty:  exchangeQty,
				QuantityDiff: math.Abs(rebuiltQty - exchangeQty),
			}
			result.MismatchedLevels = append(result.MismatchedLevels, mismatch)
			if side == SideBuy {
				result.BidMismatch++
			} else {
				result.AskMismatch++
			}

			// Track price drift
			priceDrift := math.Abs(rebuiltPrice - exchangePrice)
			if priceDrift > result.MaxPriceDrift {
				result.MaxPriceDrift = priceDrift
			}
			continue
		}

		// Check quantity within allowed drift
		qtyDiff := math.Abs(rebuiltQty - exchangeQty)
		if exchangeQty > 0 {
			percentDiff := qtyDiff / exchangeQty
			if percentDiff > v.maxAllowedDrift {
				mismatch := LevelMismatch{
					Side:           side,
					Price:          exchangePrice,
					RebuiltQty:     rebuiltQty,
					ExchangeQty:    exchangeQty,
					QuantityDiff:   qtyDiff,
					PercentageDiff: percentDiff,
				}
				result.MismatchedLevels = append(result.MismatchedLevels, mismatch)
				if side == SideBuy {
					result.BidMismatch++
					result.TotalBidDiff += qtyDiff
				} else {
					result.AskMismatch++
					result.TotalAskDiff += qtyDiff
				}
			}
		}
	}
}

// ValidateQuick performs a quick validation checking only top-level statistics.
// Returns true if the snapshots match within tolerance, false otherwise.
func (v *OrderBookValidator) ValidateQuick(rebuilt *Snapshot, exchange *ExchangeSnapshot) bool {
	if rebuilt == nil || exchange == nil {
		return false
	}
	if rebuilt.SymbolID != exchange.SymbolID {
		return false
	}

	// Check level counts
	if len(rebuilt.Bids) != len(exchange.Bids) || len(rebuilt.Asks) != len(exchange.Asks) {
		return false
	}

	// Check spread matches (allow small tolerance)
	if len(exchange.Bids) > 0 && len(exchange.Asks) > 0 {
		expectedSpread := float64(exchange.Asks[0].Price - exchange.Bids[0].Price)
		if math.Abs(rebuilt.Spread-expectedSpread) > 1.0 {
			return false
		}
	}

	return true
}
