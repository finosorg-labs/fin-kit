// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"sync"
	"time"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
)

// HFTOrderBook provides high-frequency trading order book with real-time metrics.
type HFTOrderBook struct {
	mu sync.RWMutex

	core     *coreob.OrderBook
	tickSize float64

	// Metrics calculators
	imbalance *OrderBookImbalance
	liquidity *LiquidityAnalyzer
	signalGen *SignalGenerator
	position  *PositionTracker
	risk      *RiskMonitor

	// Cached metrics
	lastImbalance *ImbalanceMetrics
	lastLiquidity *LiquidityMetrics
	lastSignal    *Signal
	lastRisk      *RiskMetrics
	lastUpdate    int64
}

// HFTOrderBookOption configures an HFT order book during creation.
type HFTOrderBookOption func(*hftOrderBookConfig)

type hftOrderBookConfig struct {
	orderCapacity int
}

// WithOrderCapacity sets the expected active order capacity for HFT order book data structures.
func WithOrderCapacity(capacity int) HFTOrderBookOption {
	return func(cfg *hftOrderBookConfig) {
		if capacity > 0 {
			cfg.orderCapacity = capacity
		}
	}
}

func int64MapCapacity(expected int) int {
	if expected <= 0 {
		return 0
	}
	return expected*10/7 + 1
}

type depthSnapshot struct {
	bestBid *PriceLevel
	bestAsk *PriceLevel
	bids    []*PriceLevel
	asks    []*PriceLevel
}

// NewHFTOrderBook creates a new HFT order book instance.
func NewHFTOrderBook(symbol string, symbolID uint32, tickSize float64, opts ...HFTOrderBookOption) *HFTOrderBook {
	cfg := hftOrderBookConfig{orderCapacity: 10000}
	for _, opt := range opts {
		opt(&cfg)
	}

	core := coreob.NewOrderBook(symbol, symbolID, coreob.WithInitialCapacity(int64MapCapacity(cfg.orderCapacity)))
	hft := &HFTOrderBook{
		core:          core,
		tickSize:      tickSize,
		lastImbalance: &ImbalanceMetrics{},
		lastLiquidity: &LiquidityMetrics{},
		lastSignal:    &Signal{Type: SignalHold},
		lastRisk:      &RiskMetrics{},
	}

	// Initialize metrics calculators
	hft.imbalance = NewOrderBookImbalance(hft)
	hft.liquidity = NewLiquidityAnalyzer(hft, tickSize)
	hft.signalGen = NewSignalGenerator()
	hft.position = NewPositionTracker()
	hft.risk = NewRiskMonitor(hft.position)

	return hft
}

// OnOrderUpdate processes an order update and generates trading signals.
func (h *HFTOrderBook) OnOrderUpdate(update *OrderUpdate) (*Signal, error) {
	if update == nil {
		return nil, ErrNilOrderUpdate
	}
	h.mu.Lock()
	defer h.mu.Unlock()

	// Update core order book
	switch update.Type {
	case OrderUpdateInsert:
		order := &coreob.Order{
			OrderID:  update.OrderID,
			Price:    update.Price,
			Quantity: update.Quantity,
			Side:     update.Side,
		}
		if err := h.core.InsertOrder(order); err != nil {
			return nil, err
		}
	case OrderUpdateDelete:
		if err := h.core.DeleteOrder(update.OrderID); err != nil {
			return nil, err
		}
	case OrderUpdateModify:
		if err := h.core.ModifyOrder(update.OrderID, update.Quantity); err != nil {
			return nil, err
		}
	}

	h.lastUpdate = update.Timestamp

	depth := h.snapshotDepth(10)
	h.imbalance.calculateFromDepth(&depth, 5, h.lastImbalance)
	h.liquidity.calculateFromDepth(&depth, h.lastLiquidity)

	// Generate signal (depends on both metrics)
	h.signalGen.generateInto(h.lastImbalance, h.lastLiquidity, update.Timestamp, h.lastSignal)

	// Update risk metrics
	h.risk.calculateInto(h.lastRisk)

	return h.lastSignal, nil
}

// GetImbalance returns the latest order book imbalance metrics.
func (h *HFTOrderBook) snapshotDepth(depth int) depthSnapshot {
	return depthSnapshot{
		bestBid: h.core.GetBestBid(),
		bestAsk: h.core.GetBestAsk(),
		bids:    h.core.GetTopNBids(depth),
		asks:    h.core.GetTopNAsks(depth),
	}
}

func (h *HFTOrderBook) GetImbalance() *ImbalanceMetrics {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if h.lastImbalance == nil {
		return &ImbalanceMetrics{}
	}
	// Return a copy to prevent external modification
	metrics := *h.lastImbalance
	return &metrics
}

// GetLiquidity returns the latest liquidity metrics.
func (h *HFTOrderBook) GetLiquidity() *LiquidityMetrics {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if h.lastLiquidity == nil {
		return &LiquidityMetrics{}
	}
	// Return a copy to prevent external modification
	metrics := *h.lastLiquidity
	return &metrics
}

// GetSignal returns the latest trading signal.
func (h *HFTOrderBook) GetSignal() *Signal {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if h.lastSignal == nil {
		return &Signal{Type: SignalHold, Timestamp: time.Now().UnixNano()}
	}
	// Return a copy to prevent external modification
	signal := *h.lastSignal
	return &signal
}

// CheckRisk returns the current risk metrics.
func (h *HFTOrderBook) CheckRisk() (*RiskMetrics, error) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if h.lastRisk == nil {
		return &RiskMetrics{}, nil
	}
	// Return a copy to prevent external modification
	metrics := *h.lastRisk
	return &metrics, nil
}

// GetCore returns the underlying core order book for direct access.
func (h *HFTOrderBook) GetCore() *coreob.OrderBook {
	return h.core
}

// Close releases resources associated with the order book.
func (h *HFTOrderBook) Close() error {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.core != nil {
		h.core.Close()
		h.core = nil
	}
	return nil
}
