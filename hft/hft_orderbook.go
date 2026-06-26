// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"sync"

	coreob "github.com/finosorg-labs/exchange-c/orderbook"
	"github.com/finosorg-labs/exchange-c/signal/tick"
)

// HFTOrderBook provides high-frequency trading order book with real-time metrics.
type HFTOrderBook struct {
	mu sync.RWMutex

	core     *coreob.OrderBook
	tickSize float64

	// Metrics calculators
	imbalance      *OrderBookImbalance
	liquidity      *LiquidityAnalyzer
	position       *PositionTracker
	risk           *RiskMonitor
	ofiCalculator  *tick.OFICalculator  // True OFI (Order Flow Imbalance) calculator
	lastOFI        float64              // Most recent OFI value

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
	hft.position = NewPositionTracker()
	hft.risk = NewRiskMonitor(hft.position)
	hft.ofiCalculator = tick.NewOFICalculator()

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

	// Only calculate metrics and signals if we have valid BBO
	if depth.bestBid != nil && depth.bestAsk != nil {
		// Calculate true OFI (Order Flow Imbalance)
		bidP := float64(depth.bestBid.Price)
		bidQ := float64(depth.bestBid.TotalQty)
		askP := float64(depth.bestAsk.Price)
		askQ := float64(depth.bestAsk.TotalQty)
		h.lastOFI = h.ofiCalculator.Update(bidP, bidQ, askP, askQ)

		// Calculate imbalance and liquidity metrics
		h.imbalance.calculateFromDepth(&depth, 5, h.lastImbalance)
		h.liquidity.calculateFromDepth(&depth, h.lastLiquidity)

		// Generate signal (now includes OFI)
		h.generateSignalWithOFI(update.Timestamp)
	} else {
		// No valid market data, reset to hold signal
		h.lastSignal.Type = SignalHold
		h.lastSignal.Strength = 0
		h.lastSignal.Confidence = 0
		h.lastSignal.Timestamp = update.Timestamp
		h.lastOFI = 0
	}

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

// generateSignalWithOFI generates trading signals using OFI and imbalance metrics
func (h *HFTOrderBook) generateSignalWithOFI(timestamp int64) {
	if h.lastImbalance == nil {
		h.lastSignal.Type = SignalHold
		h.lastSignal.Strength = 0
		h.lastSignal.Confidence = 0
		h.lastSignal.Timestamp = timestamp
		return
	}

	// Combine OFI with OrderFlowBalance for signal generation
	// OFI: positive = buying pressure, negative = selling pressure
	// OrderFlowBalance: positive = bid-heavy, negative = ask-heavy

	// Normalize OFI to [-1, 1] range (typical OFI range is ~[-500, 500] for liquid markets)
	const ofiNormalizationFactor = 100.0
	normalizedOFI := h.lastOFI / ofiNormalizationFactor
	if normalizedOFI > 1.0 {
		normalizedOFI = 1.0
	} else if normalizedOFI < -1.0 {
		normalizedOFI = -1.0
	}

	// Weighted combination: 60% OFI (dynamic), 40% OrderFlowBalance (static)
	combinedSignal := 0.6*normalizedOFI + 0.4*h.lastImbalance.OrderFlowBalance

	// Signal generation thresholds
	const buyThreshold = 0.3
	const sellThreshold = -0.3

	if combinedSignal > buyThreshold {
		h.lastSignal.Type = SignalBuy
		h.lastSignal.Strength = combinedSignal
	} else if combinedSignal < sellThreshold {
		h.lastSignal.Type = SignalSell
		h.lastSignal.Strength = -combinedSignal
	} else {
		h.lastSignal.Type = SignalHold
		h.lastSignal.Strength = 0
	}

	// Clamp strength to [0, 1]
	if h.lastSignal.Strength > 1.0 {
		h.lastSignal.Strength = 1.0
	}

	// Confidence calculation: agreement between OFI and static imbalance
	// High confidence when both point in same direction
	ofiSign := 0.0
	if normalizedOFI > 0.1 {
		ofiSign = 1.0
	} else if normalizedOFI < -0.1 {
		ofiSign = -1.0
	}

	balanceSign := 0.0
	if h.lastImbalance.OrderFlowBalance > 0.1 {
		balanceSign = 1.0
	} else if h.lastImbalance.OrderFlowBalance < -0.1 {
		balanceSign = -1.0
	}

	// Confidence: 0.5 (neutral) to 1.0 (strong agreement)
	agreement := ofiSign * balanceSign // 1.0 = agree, -1.0 = disagree, 0 = neutral
	h.lastSignal.Confidence = 0.5 + 0.3*agreement + 0.2*h.lastSignal.Strength

	if h.lastSignal.Confidence > 1.0 {
		h.lastSignal.Confidence = 1.0
	} else if h.lastSignal.Confidence < 0.0 {
		h.lastSignal.Confidence = 0.0
	}

	h.lastSignal.Timestamp = timestamp
}

// GetOFI returns the most recent OFI (Order Flow Imbalance) value
func (h *HFTOrderBook) GetOFI() float64 {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.lastOFI
}

// ResetOFI resets the OFI calculator state (useful for new trading sessions)
func (h *HFTOrderBook) ResetOFI() {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.ofiCalculator.Reset()
	h.lastOFI = 0
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
