package broker

import (
	"errors"
	"testing"
)

type mockSubscriber struct {
	snapshots []*Snapshot
	err       error
}

func (m *mockSubscriber) OnSnapshot(snapshot *Snapshot) error {
	m.snapshots = append(m.snapshots, snapshot)
	return m.err
}

func TestSnapshotPublisherSubscribe(t *testing.T) {
	publisher := NewSnapshotPublisher()

	sub := &mockSubscriber{}
	if err := publisher.Subscribe(sub); err != nil {
	t.Fatalf("Subscribe failed: %v", err)
	}

	if publisher.SubscriberCount() != 1 {
		t.Errorf("Expected 1 subscriber, got %d", publisher.SubscriberCount())
	}
}

func TestSnapshotPublisherSubscribeNil(t *testing.T) {
	publisher := NewSnapshotPublisher()
	err := publisher.Subscribe(nil)
	if err == nil {
		t.Error("Expected error for nil subscriber")
	}
	if !errors.Is(err, ErrNilSubscriber) {
		t.Errorf("Expected ErrNilSubscriber, got %v", err)
	}
}

func TestSnapshotPublisherPublish(t *testing.T) {
	publisher := NewSnapshotPublisher()

	sub1 := &mockSubscriber{}
	sub2 := &mockSubscriber{}

	publisher.Subscribe(sub1)
	publisher.Subscribe(sub2)

	snapshot := &Snapshot{SymbolID: 1}
	if err := publisher.Publish(snapshot); err != nil {
		t.Fatalf("Publish failed: %v", err)
	}

	if len(sub1.snapshots) != 1 {
		t.Errorf("Expected sub1 to receive 1 snapshot, got %d", len(sub1.snapshots))
	}
	if len(sub2.snapshots) != 1 {
		t.Errorf("Expected sub2 to receive 1 snapshot, got %d", len(sub2.snapshots))
	}
	if sub1.snapshots[0] != snapshot {
		t.Error("sub1 did not receive the correct snapshot")
	}
}

func TestSnapshotPublisherPublishFailFast(t *testing.T) {
	publisher := NewSnapshotPublisher()

	sub1 := &mockSubscriber{}
	sub2Error := &mockSubscriber{err: errors.New("subscriber error")}
	sub3 := &mockSubscriber{}

	publisher.Subscribe(sub1)
	publisher.Subscribe(sub2Error)
	publisher.Subscribe(sub3)

	snapshot := &Snapshot{SymbolID: 1}
	err := publisher.Publish(snapshot)
	if err == nil {
		t.Error("Expected error from failing subscriber")
	}

	// sub1 should receive the snapshot
	if len(sub1.snapshots) != 1 {
		t.Errorf("Expected sub1 to receive snapshot, got %d", len(sub1.snapshots))
	}

	// sub3 should NOT receive the snapshot (fail-fast)
	if len(sub3.snapshots) != 0 {
		t.Errorf("Expected sub3 to not receive snapshot due to fail-fast, got %d", len(sub3.snapshots))
	}
}

func TestSnapshotPublisherMultipleSubscribe(t *testing.T) {
	publisher := NewSnapshotPublisher()

	for i := 0; i < 5; i++ {
		publisher.Subscribe(&mockSubscriber{})
	}

	if publisher.SubscriberCount() != 5 {
		t.Errorf("Expected 5 subscribers, got %d", publisher.SubscriberCount())
	}
}
