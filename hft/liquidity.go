// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"
)

// LiquidityAnalyzer analyzes order book liquidity metrics.
type LiquidityAnalyzer struct {
	orderbook *HFTOrderBook
	tickSize  float64
}

// NewLiquidityAnalyzer creates a new liquidity analyzer.
func NewLiquidityAnalyzer(ob *HFTOrderBook, tickSize float64) *LiquidityAnalyzer {
	return &LiquidityAnalyzer{
		orderbook: ob,
		tickSize:  tickSize,
	}
}

// Calculate computes the current liquidity metrics.
// Returns zero metrics if order book is empty or has no BBO.
func (l *LiquidityAnalyzer) Calculate() *LiquidityMetrics {
	core := l.orderbook.core

	// Get best bid and ask
	bestBid := core.GetBestBid()
	bestAsk := core.GetBestAsk()
	if bestBid == nil || bestAsk == nil {
		return &LiquidityMetrics{}
	}

	// Get top N levels for depth analysis
	const topN = 10
	bids := core.GetTopNBids(topN)
	asks := core.GetTopNAsks(topN)

	// Calculate total depth
	var bidDepth, askDepth int64
	for _, level := range bids {
		bidDepth += level.TotalQty
	}
	for _, level := range asks {
		askDepth += level.TotalQty
	}

	// Spread in ticks
	spread := bestAsk.Price - bestBid.Price

	// Mid price for percentage calculations
	midPrice := float64(bestBid.Price+bestAsk.Price) / 2.0

	// Spread in basis points (bps)
	var spreadBPS float64
	if midPrice > 0 {
		spreadBPS = (float64(spread) / midPrice) * 10000.0
	}

	// Effective spread (average impact to trade both sides)
	var effectiveSpread float64
	if bidDepth > 0 && askDepth > 0 {
		effectiveSpread = float64(spread) / midPrice
	}

	// Market impact (estimated for a standard order size)
	const standardQty = 100
	marketImpact := l.calculateMarketImpact(bids, asks, standardQty)

	return &LiquidityMetrics{
		BidDepth:        bidDepth,
		AskDepth:        askDepth,
		Spread:          spread,
		SpreadBPS:       spreadBPS,
		EffectiveSpread: effectiveSpread,
		MarketImpact:    marketImpact,
	}
}

// calculateMarketImpact estimates the price impact of executing a given quantity.
func (l *LiquidityAnalyzer) calculateMarketImpact(bids, asks []*PriceLevel, qty int64) float64 {
	if len(bids) == 0 || len(asks) == 0 {
		return 0.0
	}

	midPrice := float64(bids[0].Price+asks[0].Price) / 2.0

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
