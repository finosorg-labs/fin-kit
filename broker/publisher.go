// Package broker provides broker-specific order book management functionality.
package broker

// This file implements snapshot publishing functionality for distributing
// order book snapshots to multiple subscribers.

// SnapshotPublisher manages a list of snapshot subscribers and publishes updates to them.
// Publishing uses fail-fast semantics: stops on first error.
type SnapshotPublisher struct {
	subscribers []SnapshotSubscriber
}

// NewSnapshotPublisher creates a new snapshot publisher.
func NewSnapshotPublisher() *SnapshotPublisher {
	return &SnapshotPublisher{
		subscribers: make([]SnapshotSubscriber, 0),
	}
}

// Subscribe adds a new snapshot subscriber.
// Returns ErrNilSubscriber if subscriber is nil.
func (p *SnapshotPublisher) Subscribe(subscriber SnapshotSubscriber) error {
	if subscriber == nil {
		return ErrNilSubscriber
	}
	p.subscribers = append(p.subscribers, subscriber)
	return nil
}

// Publish sends a snapshot to all subscribers.
// Stops and returns error on first subscriber error (fail-fast semantics).
func (p *SnapshotPublisher) Publish(snapshot *Snapshot) error {
	for _, subscriber := range p.subscribers {
		if err := subscriber.OnSnapshot(snapshot); err != nil {
			return err
		}
	}
	return nil
}

// SubscriberCount returns the number of registered subscribers.
func (p *SnapshotPublisher) SubscriberCount() int {
	return len(p.subscribers)
}
