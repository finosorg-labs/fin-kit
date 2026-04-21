package fin_kit

import (
	"testing"
)

func TestVersion(t *testing.T) {
	// Test version constants
	if VersionMajor != 1 {
		t.Errorf("VersionMajor = %d, want 1", VersionMajor)
	}
	if VersionMinor != 0 {
		t.Errorf("VersionMinor = %d, want 0", VersionMinor)
	}
	if VersionPatch != 0 {
		t.Errorf("VersionPatch = %d, want 0", VersionPatch)
	}
	if Version != "1.0.0" {
		t.Errorf("Version = %q, want %q", Version, "1.0.0")
	}
}
