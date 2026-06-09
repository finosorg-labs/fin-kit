// Package hft provides high-frequency trading order book functionality.
package hft

import (
	"math"
)

// Default signal generation thresholds
const (
	DefaultImbalanceThreshold = 0.3  // 30% order flow imbalance triggers buy/sell signal
	DefaultLiquidityThreshold = 0.01 // 1% market impact threshold for confidence calculation
	DefaultStrengthDecay      = 0.95 // Signal strength decay factor per time unit
)

// SignalGenerator generates trading signals from order book metrics.
type SignalGenerator struct {
	// Thresholds for signal generation
	imbalanceThreshold float64
	liquidityThreshold float64
	strengthDecay      float64
}

// NewSignalGenerator creates a new signal generator with default thresholds.
func NewSignalGenerator() *SignalGenerator {
	return &SignalGenerator{
		imbalanceThreshold: DefaultImbalanceThreshold,
		liquidityThreshold: DefaultLiquidityThreshold,
		strengthDecay:      DefaultStrengthDecay,
	}
}

// Generate creates a trading signal based on imbalance and liquidity metrics.
func (s *SignalGenerator) Generate(
	imbalance *ImbalanceMetrics,
	liquidity *LiquidityMetrics,
	timestamp int64,
) *Signal {
	signal := &Signal{}
	s.generateInto(imbalance, liquidity, timestamp, signal)
	return signal
}

func (s *SignalGenerator) generateInto(
	imbalance *ImbalanceMetrics,
	liquidity *LiquidityMetrics,
	timestamp int64,
	out *Signal,
) {
	*out = Signal{Type: SignalHold, Timestamp: timestamp}
	if imbalance == nil || liquidity == nil {
		return
	}

	// Determine signal type based on order flow balance
	if imbalance.OrderFlowBalance > s.imbalanceThreshold {
		out.Type = SignalBuy
	} else if imbalance.OrderFlowBalance < -s.imbalanceThreshold {
		out.Type = SignalSell
	}

	// Calculate signal strength based on imbalance magnitude
	out.Strength = math.Abs(imbalance.OrderFlowBalance)
	if out.Strength > 1.0 {
		out.Strength = 1.0
	}

	// Calculate confidence based on multiple factors
	out.Confidence = s.calculateConfidence(imbalance, liquidity)
}

// calculateConfidence computes signal confidence from multiple indicators.
func (s *SignalGenerator) calculateConfidence(
	imbalance *ImbalanceMetrics,
	liquidity *LiquidityMetrics,
) float64 {
	// Factor 1: Alignment between imbalance indicators
	alignmentScore := s.calculateAlignmentScore(imbalance)

	// Factor 2: Liquidity quality (tight spread, good depth)
	liquidityScore := s.calculateLiquidityScore(liquidity)

	// Factor 3: Price pressure strength
	pressureScore := math.Abs(imbalance.PricePressure)

	// Weighted average
	confidence := (alignmentScore*0.4 + liquidityScore*0.3 + pressureScore*0.3)

	// Clamp to [0, 1]
	if confidence > 1.0 {
		confidence = 1.0
	}
	if confidence < 0.0 {
		confidence = 0.0
	}

	return confidence
}

// calculateAlignmentScore measures agreement between imbalance indicators.
func (s *SignalGenerator) calculateAlignmentScore(imbalance *ImbalanceMetrics) float64 {
	// All indicators should point in the same direction
	bidAskSign := sign(imbalance.BidAskImbalance)
	volumeSign := sign(imbalance.VolumeImbalance)
	pressureSign := sign(imbalance.PricePressure)

	// Count agreements
	agreements := 0
	if bidAskSign == volumeSign {
		agreements++
	}
	if bidAskSign == pressureSign {
		agreements++
	}
	if volumeSign == pressureSign {
		agreements++
	}

	// Score: 0 (no agreement) to 1 (full agreement)
	return float64(agreements) / 3.0
}

// calculateLiquidityScore assesses order book liquidity quality.
func (s *SignalGenerator) calculateLiquidityScore(liquidity *LiquidityMetrics) float64 {
	// Good liquidity = tight spread + balanced depth + low market impact
	spreadScore := 1.0 - math.Min(liquidity.SpreadBPS/100.0, 1.0)
	depthScore := depthBalance(liquidity.BidDepth, liquidity.AskDepth)
	impactScore := 1.0 - math.Min(liquidity.MarketImpact/s.liquidityThreshold, 1.0)

	return (spreadScore + depthScore + impactScore) / 3.0
}

// GenerateWithThresholds creates a signal with custom thresholds.
func (s *SignalGenerator) GenerateWithThresholds(
	imbalance *ImbalanceMetrics,
	liquidity *LiquidityMetrics,
	timestamp int64,
	imbalanceThresh, liquidityThresh float64,
) *Signal {
	if imbalance == nil || liquidity == nil {
		return &Signal{
			Type:       SignalHold,
			Strength:   0.0,
			Confidence: 0.0,
			Timestamp:  timestamp,
		}
	}

	// Determine signal type based on order flow balance
	var signalType SignalType
	if imbalance.OrderFlowBalance > imbalanceThresh {
		signalType = SignalBuy
	} else if imbalance.OrderFlowBalance < -imbalanceThresh {
		signalType = SignalSell
	} else {
		signalType = SignalHold
	}

	// Calculate signal strength based on imbalance magnitude
	strength := math.Abs(imbalance.OrderFlowBalance)
	if strength > 1.0 {
		strength = 1.0
	}

	// Calculate confidence with custom thresholds
	confidence := s.calculateConfidenceWithThresholds(imbalance, liquidity, liquidityThresh)

	return &Signal{
		Type:       signalType,
		Strength:   strength,
		Confidence: confidence,
		Timestamp:  timestamp,
	}
}

// calculateConfidenceWithThresholds computes signal confidence with custom liquidity threshold.
func (s *SignalGenerator) calculateConfidenceWithThresholds(
	imbalance *ImbalanceMetrics,
	liquidity *LiquidityMetrics,
	liquidityThresh float64,
) float64 {
	// Factor 1: Alignment between imbalance indicators
	alignmentScore := s.calculateAlignmentScore(imbalance)

	// Factor 2: Liquidity quality (tight spread, good depth)
	liquidityScore := s.calculateLiquidityScoreWithThreshold(liquidity, liquidityThresh)

	// Factor 3: Price pressure strength
	pressureScore := math.Abs(imbalance.PricePressure)

	// Weighted average
	confidence := (alignmentScore*0.4 + liquidityScore*0.3 + pressureScore*0.3)

	// Clamp to [0, 1]
	if confidence > 1.0 {
		confidence = 1.0
	}
	if confidence < 0.0 {
		confidence = 0.0
	}

	return confidence
}

// calculateLiquidityScoreWithThreshold assesses order book liquidity quality with custom threshold.
func (s *SignalGenerator) calculateLiquidityScoreWithThreshold(liquidity *LiquidityMetrics, liquidityThresh float64) float64 {
	// Good liquidity = tight spread + balanced depth + low market impact
	spreadScore := 1.0 - math.Min(liquidity.SpreadBPS/100.0, 1.0)
	depthScore := depthBalance(liquidity.BidDepth, liquidity.AskDepth)
	impactScore := 1.0 - math.Min(liquidity.MarketImpact/liquidityThresh, 1.0)

	return (spreadScore + depthScore + impactScore) / 3.0
}

// SetThresholds updates the signal generation thresholds.
func (s *SignalGenerator) SetThresholds(imbalanceThresh, liquidityThresh float64) {
	s.imbalanceThreshold = imbalanceThresh
	s.liquidityThreshold = liquidityThresh
}

// Helper functions

// sign returns the sign of a number: -1, 0, or 1.
func sign(x float64) int {
	if x > 0 {
		return 1
	} else if x < 0 {
		return -1
	}
	return 0
}

// depthBalance calculates balance between bid and ask depth.
func depthBalance(bidDepth, askDepth int64) float64 {
	total := bidDepth + askDepth
	if total == 0 {
		return 0.0
	}

	// Perfect balance = 0.5/0.5 = score 1.0
	// Imbalanced = 0.9/0.1 or 0.1/0.9 = score closer to 0
	ratio := float64(bidDepth) / float64(total)
	imbalance := math.Abs(ratio - 0.5)

	return 1.0 - (imbalance * 2.0)
}
