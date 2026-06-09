// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"
	"sort"
	"sync"
)

// RiskMonitor monitors risk metrics for trading positions.
type RiskMonitor struct {
	mu              sync.RWMutex
	position        *PositionTracker
	maxDrawdown     float64
	highWaterMark   float64
	confidenceLevel float64 // VaR confidence level (default 0.95)
	priceHistory    []float64
	maxHistorySize  int
}

// NewRiskMonitor creates a new risk monitor.
func NewRiskMonitor(position *PositionTracker) *RiskMonitor {
	return &RiskMonitor{
		position:        position,
		confidenceLevel: 0.95,
		priceHistory:    make([]float64, 0, 100),
		maxHistorySize:  100,
	}
}

// Calculate computes current risk metrics.
func (r *RiskMonitor) Calculate() *RiskMetrics {
	metrics := &RiskMetrics{}
	r.calculateInto(metrics)
	return metrics
}

func (r *RiskMonitor) calculateInto(out *RiskMetrics) {
	r.mu.Lock()
	defer r.mu.Unlock()

	position := r.position.GetPosition()
	exposure := r.position.GetExposure()
	totalPnL := r.position.GetTotalPnL()

	// Update high water mark and max drawdown
	if totalPnL > r.highWaterMark {
		r.highWaterMark = totalPnL
	}

	drawdown := r.highWaterMark - totalPnL
	if drawdown > r.maxDrawdown {
		r.maxDrawdown = drawdown
	}

	// Calculate VaR if we have price history
	var varValue float64
	if len(r.priceHistory) > 1 {
		varValue = r.calculateVaR()
	}

	// Expected profit (simple estimate based on unrealized PnL)
	expectedProfit := r.position.GetUnrealizedPnL()

	out.Position = position
	out.Exposure = math.Abs(exposure)
	out.MaxDrawdown = r.maxDrawdown
	out.VaR = varValue
	out.ExpectedProfit = expectedProfit
}

// UpdatePriceHistory adds a new price to the historical data.
func (r *RiskMonitor) UpdatePriceHistory(price float64) {
	r.mu.Lock()
	defer r.mu.Unlock()

	r.priceHistory = append(r.priceHistory, price)
	// Keep only recent history
	if len(r.priceHistory) > r.maxHistorySize {
		r.priceHistory = r.priceHistory[len(r.priceHistory)-r.maxHistorySize:]
	}
}

// calculateVaR computes Value at Risk using historical simulation.
func (r *RiskMonitor) calculateVaR() float64 {
	if len(r.priceHistory) < 2 {
		return 0.0
	}

	// Calculate returns
	returns := make([]float64, len(r.priceHistory)-1)
	for i := 1; i < len(r.priceHistory); i++ {
		returns[i-1] = (r.priceHistory[i] - r.priceHistory[i-1]) / r.priceHistory[i-1]
	}

	// Sort returns
	sortedReturns := make([]float64, len(returns))
	copy(sortedReturns, returns)
	sort.Float64s(sortedReturns)

	// Get the return at the confidence level percentile
	idx := int(float64(len(sortedReturns)) * (1.0 - r.confidenceLevel))
	if idx < 0 {
		idx = 0
	}
	if idx >= len(sortedReturns) {
		idx = len(sortedReturns) - 1
	}

	// VaR is the loss at this percentile
	varReturn := sortedReturns[idx]

	// Convert to absolute value
	position := r.position.GetPosition()
	exposure := r.position.GetExposure()

	var varValue float64
	if position != 0 && len(r.priceHistory) > 0 {
		currentPrice := r.priceHistory[len(r.priceHistory)-1]
		varValue = math.Abs(varReturn * currentPrice * float64(position))
	} else {
		varValue = math.Abs(varReturn * exposure)
	}

	return varValue
}

// SetConfidenceLevel sets the VaR confidence level.
func (r *RiskMonitor) SetConfidenceLevel(level float64) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if level > 0.0 && level < 1.0 {
		r.confidenceLevel = level
	}
}

// GetMaxDrawdown returns the maximum drawdown observed.
func (r *RiskMonitor) GetMaxDrawdown() float64 {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.maxDrawdown
}

// ResetDrawdown resets the drawdown tracking.
func (r *RiskMonitor) ResetDrawdown() {
	r.mu.Lock()
	defer r.mu.Unlock()

	r.maxDrawdown = 0.0
	r.highWaterMark = r.position.GetTotalPnL()
}

// CheckRiskLimits checks if risk metrics exceed specified limits.
func (r *RiskMonitor) CheckRiskLimits(maxExposure, maxDrawdown, maxVaR float64) bool {
	metrics := r.Calculate()

	if metrics.Exposure > maxExposure {
		return false
	}
	if metrics.MaxDrawdown > maxDrawdown {
		return false
	}
	if metrics.VaR > maxVaR {
		return false
	}

	return true
}
