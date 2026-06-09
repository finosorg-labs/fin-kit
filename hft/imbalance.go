// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// OrderBookImbalance calculates order book imbalance metrics.
type OrderBookImbalance struct {
	orderbook *HFTOrderBook
}

// NewOrderBookImbalance creates a new order book imbalance calculator.
func NewOrderBookImbalance(ob *HFTOrderBook) *OrderBookImbalance {
	return &OrderBookImbalance{
		orderbook: ob,
	}
}

// Calculate computes the current order book imbalance metrics.
// Returns zero metrics if order book is empty or has no BBO.
func (i *OrderBookImbalance) Calculate() *ImbalanceMetrics {
	return i.CalculateWithDepth(5)
}

// CalculateWithDepth computes imbalance metrics using custom depth levels.
func (i *OrderBookImbalance) CalculateWithDepth(depth int) *ImbalanceMetrics {
	if depth <= 0 {
		depth = 5
	}

	snapshot := depthSnapshot{
		bestBid: i.orderbook.core.GetBestBid(),
		bestAsk: i.orderbook.core.GetBestAsk(),
		bids:    i.orderbook.core.GetTopNBids(depth),
		asks:    i.orderbook.core.GetTopNAsks(depth),
	}
	metrics := &ImbalanceMetrics{}
	i.calculateFromDepth(&snapshot, depth, metrics)
	return metrics
}

func (i *OrderBookImbalance) calculateFromDepth(snapshot *depthSnapshot, depth int, out *ImbalanceMetrics) {
	*out = ImbalanceMetrics{}
	if depth <= 0 {
		depth = 5
	}
	if snapshot.bestBid == nil || snapshot.bestAsk == nil {
		return
	}

	bids := snapshot.bids
	if len(bids) > depth {
		bids = bids[:depth]
	}
	asks := snapshot.asks
	if len(asks) > depth {
		asks = asks[:depth]
	}

	var bidQty, askQty int64
	for _, level := range bids {
		bidQty += level.TotalQty
	}
	for _, level := range asks {
		askQty += level.TotalQty
	}

	totalQty := bidQty + askQty
	if totalQty <= 0 {
		return
	}

	bidAskImbalance := float64(bidQty-askQty) / float64(totalQty)
	midPrice := (float64(snapshot.bestBid.Price) + float64(snapshot.bestAsk.Price)) * 0.5
	if midPrice <= 0 {
		return
	}

	var weightedBidQty, weightedAskQty float64

	for _, level := range bids {
		distance := math.Abs(float64(level.Price) - midPrice)
		weight := 1.0 / (1.0 + distance/midPrice)
		weightedBidQty += float64(level.TotalQty) * weight
	}

	for _, level := range asks {
		distance := math.Abs(float64(level.Price) - midPrice)
		weight := 1.0 / (1.0 + distance/midPrice)
		weightedAskQty += float64(level.TotalQty) * weight
	}

	var volumeImbalance float64
	if weightedBidQty+weightedAskQty > 0 {
		volumeImbalance = (weightedBidQty - weightedAskQty) / (weightedBidQty + weightedAskQty)
	}

	var pricePressure float64
	if bidQty > askQty {
		pricePressure = float64(bidQty) / float64(totalQty)
	} else {
		pricePressure = -float64(askQty) / float64(totalQty)
	}

	out.BidAskImbalance = bidAskImbalance
	out.VolumeImbalance = volumeImbalance
	out.PricePressure = pricePressure
	out.OrderFlowBalance = (bidAskImbalance + volumeImbalance) * 0.5
}

// GetBestBid is a helper to access best bid from core.
func (i *OrderBookImbalance) GetBestBid() *coreob.PriceLevel {
	return i.orderbook.core.GetBestBid()
}

// GetBestAsk is a helper to access best ask from core.
func (i *OrderBookImbalance) GetBestAsk() *coreob.PriceLevel {
	return i.orderbook.core.GetBestAsk()
}
