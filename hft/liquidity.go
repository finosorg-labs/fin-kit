// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"
)

// LiquidityAnalyzer analyzes order book liquidity metrics.
type LiquidityAnalyzer struct {
	orderbook   *HFTOrderBook
	tickSize    float64
	standardQty int64 // Standard order size for market impact calculation
}

// NewLiquidityAnalyzer creates a new liquidity analyzer.
func NewLiquidityAnalyzer(ob *HFTOrderBook, tickSize float64) *LiquidityAnalyzer {
	return &LiquidityAnalyzer{
		orderbook:   ob,
		tickSize:    tickSize,
		standardQty: 100, // Default standard quantity
	}
}

// Calculate computes the current liquidity metrics.
// Returns zero metrics if order book is empty or has no BBO.
func (l *LiquidityAnalyzer) Calculate() *LiquidityMetrics {
	const topN = 10
	snapshot := depthSnapshot{
		bestBid: l.orderbook.core.GetBestBid(),
		bestAsk: l.orderbook.core.GetBestAsk(),
		bids:    l.orderbook.core.GetTopNBids(topN),
		asks:    l.orderbook.core.GetTopNAsks(topN),
	}
	metrics := &LiquidityMetrics{}
	l.calculateFromDepth(&snapshot, metrics)
	return metrics
}

func (l *LiquidityAnalyzer) calculateFromDepth(snapshot *depthSnapshot, out *LiquidityMetrics) {
	*out = LiquidityMetrics{}
	if snapshot.bestBid == nil || snapshot.bestAsk == nil {
		return
	}

	var bidDepth, askDepth int64
	for _, level := range snapshot.bids {
		bidDepth += level.TotalQty
	}
	for _, level := range snapshot.asks {
		askDepth += level.TotalQty
	}

	spread := snapshot.bestAsk.Price - snapshot.bestBid.Price
	midPrice := (float64(snapshot.bestBid.Price) + float64(snapshot.bestAsk.Price)) * 0.5

	var spreadBPS float64
	if midPrice > 0 {
		spreadBPS = (float64(spread) / midPrice) * 10000.0
	}

	var effectiveSpread float64
	if bidDepth > 0 && askDepth > 0 && midPrice > 0 {
		effectiveSpread = float64(spread) / midPrice
	}

	out.BidDepth = bidDepth
	out.AskDepth = askDepth
	out.Spread = spread
	out.SpreadBPS = spreadBPS
	out.EffectiveSpread = effectiveSpread
	out.MarketImpact = l.calculateMarketImpact(snapshot.bids, snapshot.asks, l.standardQty, midPrice)
}

// calculateMarketImpact estimates the price impact of executing a given quantity.
func (l *LiquidityAnalyzer) calculateMarketImpact(bids, asks []*PriceLevel, qty int64, midPrice float64) float64 {
	// Calculate average execution price for buying qty
	remaining := qty
	var totalCost int64
	for _, level := range asks {
		if remaining <= 0 {
			break
		}
		filled := remaining
		if filled > level.TotalQty {
			filled = level.TotalQty
		}
		totalCost += filled * level.Price
		remaining -= filled
	}

	// If we couldn't fill the order, return max impact
	if remaining > 0 {
		return 1.0
	}

	avgPrice := float64(totalCost) / float64(qty)
	impact := math.Abs(avgPrice-midPrice) / midPrice

	return impact
}

// SetStandardQuantity sets the standard order size for market impact calculation.
func (l *LiquidityAnalyzer) SetStandardQuantity(qty int64) {
	if qty > 0 {
		l.standardQty = qty
	}
}

// CalculateWithQuantity computes market impact for a specific quantity.
func (l *LiquidityAnalyzer) CalculateWithQuantity(qty int64, side Side) float64 {
	core := l.orderbook.core

	bestBid := core.GetBestBid()
	bestAsk := core.GetBestAsk()
	if bestBid == nil || bestAsk == nil {
		return 0.0
	}

	const topN = 20
	midPrice := float64(bestBid.Price+bestAsk.Price) / 2.0

	if side == SideBuy {
		// Buying walks up the ask side
		asks := core.GetTopNAsks(topN)
		return l.calculateSideImpact(asks, qty, midPrice)
	}

	// Selling walks down the bid side
	bids := core.GetTopNBids(topN)
	return l.calculateSideImpact(bids, qty, midPrice)
}

// calculateSideImpact computes impact on one side of the book.
func (l *LiquidityAnalyzer) calculateSideImpact(levels []*PriceLevel, qty int64, midPrice float64) float64 {
	if len(levels) == 0 {
		return 1.0
	}

	remaining := qty
	var totalCost int64

	for _, level := range levels {
		if remaining <= 0 {
			break
		}
		filled := remaining
		if filled > level.TotalQty {
			filled = level.TotalQty
		}
		totalCost += filled * level.Price
		remaining -= filled
	}

	if remaining > 0 {
		return 1.0
	}

	avgPrice := float64(totalCost) / float64(qty)
	impact := math.Abs(avgPrice-midPrice) / midPrice

	return impact
}

// GetDepthProfile returns quantity available at each price level up to maxDepth.
func (l *LiquidityAnalyzer) GetDepthProfile(maxDepth int, side Side) []*PriceLevel {
	core := l.orderbook.core

	if side == SideBuy {
		return core.GetTopNBids(maxDepth)
	}
	return core.GetTopNAsks(maxDepth)
}
