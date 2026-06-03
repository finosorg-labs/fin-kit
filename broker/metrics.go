// Package broker provides broker-specific order book management functionality.
package broker

// This file implements broker-specific metrics for order book monitoring.

import (
	"sync/atomic"
	"time"
)

// BrokerMetrics tracks order book update and snapshot generation metrics.
// All methods are thread-safe using atomic operations.
//
// Metrics tracked:
//   - Update count: Total number of order book updates processed
//   - Snapshot count: Total number of snapshots generated
//   - Last update timestamp: Timestamp of the most recent update
type BrokerMetrics struct {
	updateCount     atomic.Int64 // Total number of order book updates
	snapshotCount   atomic.Int64 // Total number of snapshots generated
	lastUpdateNanos atomic.Int64 // Timestamp of last update in nanoseconds
}

// UpdateCount returns the total number of order book updates processed.
func (m *BrokerMetrics) UpdateCount() int64 {
	return m.updateCount.Load()
}

// SnapshotCount returns the total number of snapshots generated.
func (m *BrokerMetrics) SnapshotCount() int64 {
	return m.snapshotCount.Load()
}

// LastUpdate returns the timestamp of the last order book update.
// Returns zero time if no updates have been processed.
func (m *BrokerMetrics) LastUpdate() time.Time {
	nanos := m.lastUpdateNanos.Load()
	if nanos == 0 {
		return time.Time{}
	}
	return time.Unix(0, nanos)
}

// RecordUpdate records an order book update.
// Increments the update counter and updates the last update timestamp.
func (m *BrokerMetrics) RecordUpdate() {
	m.updateCount.Add(1)
	m.lastUpdateNanos.Store(time.Now().UnixNano())
}

// RecordSnapshot records a snapshot generation.
// Increments the snapshot counter.
func (m *BrokerMetrics) RecordSnapshot() {
	m.snapshotCount.Add(1)
}
