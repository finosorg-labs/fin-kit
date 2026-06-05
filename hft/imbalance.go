// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
	"github.com/finosorg-labs/platform"
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

	core := i.orderbook.core

	bestBid := core.GetBestBid()
	bestAsk := core.GetBestAsk()

	if bestBid == nil || bestAsk == nil {
		return &ImbalanceMetrics{}
	}

	bids := core.GetTopNBids(depth)
	asks := core.GetTopNAsks(depth)

	var bidQty, askQty int64
	for _, level := range bids {
		bidQty += level.TotalQty
	}
	for _, level := range asks {
		askQty += level.TotalQty
	}

	var bidAskImbalance float64
	if bidQty+askQty > 0 {
		bidAskImbalance = float64(bidQty-askQty) / float64(bidQty+askQty)
	}

	// Calculate midPrice using BigFloat for precision
	bidPrice, err := platform.NewBigFloat()
	if err != nil {
		return &ImbalanceMetrics{}
	}
	defer bidPrice.Destroy()

	askPrice, err := platform.NewBigFloat()
	if err != nil {
		return &ImbalanceMetrics{}
	}
	defer askPrice.Destroy()

	sum, err := platform.NewBigFloat()
	if err != nil {
		return &ImbalanceMetrics{}
	}
	defer sum.Destroy()

	two, err := platform.NewBigFloat()
	if err != nil {
		return &ImbalanceMetrics{}
	}
	defer two.Destroy()

	result, err := platform.NewBigFloat()
	if err != nil {
		return &ImbalanceMetrics{}
	}
	defer result.Destroy()

	if err := bidPrice.SetInt64(bestBid.Price); err != nil {
		return &ImbalanceMetrics{}
	}
	if err := askPrice.SetInt64(bestAsk.Price); err != nil {
		return &ImbalanceMetrics{}
	}
	if err := sum.Add(bidPrice, askPrice); err != nil {
		return &ImbalanceMetrics{}
	}
	if err := two.SetFloat64(2.0); err != nil {
		return &ImbalanceMetrics{}
	}
	if err := result.Div(sum, two); err != nil {
		return &ImbalanceMetrics{}
	}

	midPrice, err := result.Float64()
	if err != nil {
		return &ImbalanceMetrics{}
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
		pricePressure = float64(bidQty) / float64(bidQty+askQty)
	} else {
		pricePressure = -float64(askQty) / float64(bidQty+askQty)
	}

	orderFlowBalance := (bidAskImbalance + volumeImbalance) / 2.0

	return &ImbalanceMetrics{
		BidAskImbalance:  bidAskImbalance,
		VolumeImbalance:  volumeImbalance,
		PricePressure:    pricePressure,
		OrderFlowBalance: orderFlowBalance,
	}
}

// GetBestBid is a helper to access best bid from core.
func (i *OrderBookImbalance) GetBestBid() *coreob.PriceLevel {
	return i.orderbook.core.GetBestBid()
}

// GetBestAsk is a helper to access best ask from core.
func (i *OrderBookImbalance) GetBestAsk() *coreob.PriceLevel {
	return i.orderbook.core.GetBestAsk()
}
