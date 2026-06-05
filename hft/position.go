// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"sync"

	"github.com/finosorg-labs/platform"
)

// PositionTracker tracks trading positions and exposure.
type PositionTracker struct {
	mu            sync.RWMutex
	position      int64   // Current position (positive = long, negative = short)
	avgEntryPrice float64 // Average entry price
	realizedPnL   float64 // Realized profit and loss
	unrealizedPnL float64 // Unrealized profit and loss
	totalVolume   int64   // Total traded volume
	longVolume    int64   // Long side volume
	shortVolume   int64   // Short side volume
}

// NewPositionTracker creates a new position tracker.
func NewPositionTracker() *PositionTracker {
	return &PositionTracker{}
}

// UpdatePosition updates the position with a new trade.
func (p *PositionTracker) UpdatePosition(side Side, quantity int64, price float64) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if side == SideBuy {
		p.updateBuy(quantity, price)
	} else {
		p.updateSell(quantity, price)
	}

	p.totalVolume += quantity
}

// updateBuy processes a buy trade.
func (p *PositionTracker) updateBuy(quantity int64, price float64) {
	p.longVolume += quantity

	if p.position < 0 {
		// Closing short position
		closeQty := quantity
		if closeQty > -p.position {
			closeQty = -p.position
		}

		// Realize PnL on closed portion
		p.realizedPnL += float64(closeQty) * (p.avgEntryPrice - price)
		p.position += closeQty

		// If still buying after closing short, update position
		remaining := quantity - closeQty
		if remaining > 0 {
			p.avgEntryPrice = price
			p.position += remaining
		} else if p.position == 0 {
			p.avgEntryPrice = 0.0
		}
	} else {
		// Adding to long or opening long
		// Use BigFloat for precision
		posVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer posVal.Destroy()

		avgPrice, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer avgPrice.Destroy()

		qtyVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer qtyVal.Destroy()

		priceVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer priceVal.Destroy()

		cost1, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer cost1.Destroy()

		cost2, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer cost2.Destroy()

		totalCost, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer totalCost.Destroy()

		newPos, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer newPos.Destroy()

		result, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer result.Destroy()

		if err := posVal.SetInt64(p.position); err != nil {
			return
		}
		if err := avgPrice.SetFloat64(p.avgEntryPrice); err != nil {
			return
		}
		if err := qtyVal.SetInt64(quantity); err != nil {
			return
		}
		if err := priceVal.SetFloat64(price); err != nil {
			return
		}

		// totalCost = position * avgEntryPrice + quantity * price
		if err := cost1.Mul(posVal, avgPrice); err != nil {
			return
		}
		if err := cost2.Mul(qtyVal, priceVal); err != nil {
			return
		}
		if err := totalCost.Add(cost1, cost2); err != nil {
			return
		}

		p.position += quantity
		if p.position > 0 {
			if err := newPos.SetInt64(p.position); err != nil {
				return
			}
			if err := result.Div(totalCost, newPos); err != nil {
				return
			}
			if p.avgEntryPrice, err = result.Float64(); err != nil {
				return
			}
		}
	}
}

// updateSell processes a sell trade.
func (p *PositionTracker) updateSell(quantity int64, price float64) {
	p.shortVolume += quantity

	if p.position > 0 {
		// Closing long position
		closeQty := quantity
		if closeQty > p.position {
			closeQty = p.position
		}

		// Realize PnL on closed portion
		p.realizedPnL += float64(closeQty) * (price - p.avgEntryPrice)
		p.position -= closeQty

		// If still selling after closing long, update position
		remaining := quantity - closeQty
		if remaining > 0 {
			p.avgEntryPrice = price
			p.position -= remaining
		} else if p.position == 0 {
			p.avgEntryPrice = 0.0
		}
	} else {
		// Adding to short or opening short
		// Use BigFloat for precision
		posVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer posVal.Destroy()

		avgPrice, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer avgPrice.Destroy()

		qtyVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer qtyVal.Destroy()

		priceVal, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer priceVal.Destroy()

		cost1, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer cost1.Destroy()

		cost2, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer cost2.Destroy()

		totalCost, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer totalCost.Destroy()

		newPos, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer newPos.Destroy()

		result, err := platform.NewBigFloat()
		if err != nil {
			return
		}
		defer result.Destroy()

		if err := posVal.SetInt64(-p.position); err != nil {
			return
		}
		if err := avgPrice.SetFloat64(p.avgEntryPrice); err != nil {
			return
		}
		if err := qtyVal.SetInt64(quantity); err != nil {
			return
		}
		if err := priceVal.SetFloat64(price); err != nil {
			return
		}

		// totalCost = -position * avgEntryPrice + quantity * price
		if err := cost1.Mul(posVal, avgPrice); err != nil {
			return
		}
		if err := cost2.Mul(qtyVal, priceVal); err != nil {
			return
		}
		if err := totalCost.Add(cost1, cost2); err != nil {
			return
		}

		p.position -= quantity
		if p.position < 0 {
			if err := newPos.SetInt64(-p.position); err != nil {
				return
			}
			if err := result.Div(totalCost, newPos); err != nil {
				return
			}
			if p.avgEntryPrice, err = result.Float64(); err != nil {
				return
			}
		}
	}
}

// UpdateUnrealizedPnL updates the unrealized PnL based on current market price.
func (p *PositionTracker) UpdateUnrealizedPnL(currentPrice float64) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.position > 0 {
		// Long position
		p.unrealizedPnL = float64(p.position) * (currentPrice - p.avgEntryPrice)
	} else if p.position < 0 {
		// Short position
		p.unrealizedPnL = float64(-p.position) * (p.avgEntryPrice - currentPrice)
	} else {
		p.unrealizedPnL = 0.0
	}
}

// GetPosition returns the current position quantity.
func (p *PositionTracker) GetPosition() int64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.position
}

// GetAvgEntryPrice returns the average entry price.
func (p *PositionTracker) GetAvgEntryPrice() float64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.avgEntryPrice
}

// GetRealizedPnL returns the realized profit and loss.
func (p *PositionTracker) GetRealizedPnL() float64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.realizedPnL
}

// GetUnrealizedPnL returns the unrealized profit and loss.
func (p *PositionTracker) GetUnrealizedPnL() float64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.unrealizedPnL
}

// GetTotalPnL returns the total profit and loss (realized + unrealized).
func (p *PositionTracker) GetTotalPnL() float64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.realizedPnL + p.unrealizedPnL
}

// GetVolume returns trading volume statistics.
func (p *PositionTracker) GetVolume() (total, long, short int64) {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.totalVolume, p.longVolume, p.shortVolume
}

// GetExposure returns the current position exposure in price terms.
func (p *PositionTracker) GetExposure() float64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return float64(p.position) * p.avgEntryPrice
}

// Reset clears all position tracking data.
func (p *PositionTracker) Reset() {
	p.mu.Lock()
	defer p.mu.Unlock()

	p.position = 0
	p.avgEntryPrice = 0.0
	p.realizedPnL = 0.0
	p.unrealizedPnL = 0.0
	p.totalVolume = 0
	p.longVolume = 0
	p.shortVolume = 0
}
