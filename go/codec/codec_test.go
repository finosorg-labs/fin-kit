package codec

import (
	"bytes"
	"testing"
)

func TestChecksum(t *testing.T) {
	tests := []struct {
		name string
		data []byte
	}{
		{
			name: "empty",
			data: []byte{},
		},
		{
			name: "simple",
			data: []byte("8=FIX.4.4\x019=40\x01"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := Checksum(tt.data)
			if tt.name == "empty" && result != 0 {
				t.Errorf("Checksum() = %d, want 0", result)
			}
			if tt.name == "simple" && result == 0 {
				t.Errorf("Checksum() = 0, want non-zero")
			}
			t.Logf("Checksum for %q: %d", tt.name, result)
		})
	}
}

func TestSIMDLevel(t *testing.T) {
	level := SIMDLevel()
	t.Logf("SIMD level: %s", level)

	validLevels := []string{"AVX2", "SSE4.2", "Scalar (no SIMD)", "Unknown"}
	found := false
	for _, valid := range validLevels {
		if level == valid {
			found = true
			break
		}
	}

	if !found {
		t.Errorf("SIMDLevel() returned unexpected value: %s", level)
	}
}

func TestParseOne(t *testing.T) {
	// Valid FIX message
	msg := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"

	result, err := ParseOne([]byte(msg))
	if err != nil {
		t.Fatalf("ParseOne() error = %v", err)
	}

	if result == nil {
		t.Fatal("ParseOne() returned nil result")
	}

	if result.FieldCount == 0 {
		t.Error("ParseOne() returned 0 fields")
	}

	t.Logf("Parsed message: %d fields", result.FieldCount)
	t.Logf("Computed checksum: %d, Declared: %d", result.ComputedChecksum%256, result.DeclaredChecksum)
	t.Logf("Computed body length: %d, Declared: %d", result.ComputedBodyLength, result.DeclaredBodyLength)
}

func TestParseOneEmpty(t *testing.T) {
	result, err := ParseOne([]byte{})
	if err == nil {
		t.Error("ParseOne() with empty data should return error")
	}
	if result != nil {
		t.Error("ParseOne() with empty data should return nil result")
	}
}

func TestParseOneNoMessage(t *testing.T) {
	result, err := ParseOne([]byte("no fix message here"))
	if err != nil {
		t.Fatalf("ParseOne() error = %v", err)
	}
	if result != nil {
		t.Error("ParseOne() with no message should return nil result")
	}
}

func TestParseMessages(t *testing.T) {
	// Two valid FIX messages
	msg1 := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"
	msg2 := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"
	data := []byte(msg1 + msg2)

	count, result, err := ParseMessages(data)
	if err != nil {
		t.Fatalf("ParseMessages() error = %v", err)
	}

	if count == 0 {
		t.Fatal("ParseMessages() returned 0 messages")
	}

	if result == nil {
		t.Fatal("ParseMessages() returned nil result")
	}

	t.Logf("Parsed %d messages, consumed %d bytes", result.MsgCount, result.ConsumedBytes)

	for i, msg := range result.Messages {
		t.Logf("Message %d: %d fields, offset=%d, length=%d",
			i, msg.FieldCount, msg.MsgOffset, msg.MsgLength)
	}
}

func TestMarshalMessage(t *testing.T) {
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
	}

	outputBuf := make([]byte, 256)
	result, err := MarshalMessage("FIX.4.4", fields, outputBuf)
	if err != nil {
		t.Fatalf("MarshalMessage() error = %v", err)
	}

	if !result.Success {
		t.Fatal("MarshalMessage() failed")
	}

	if result.OutputLength == 0 {
		t.Error("MarshalMessage() returned 0 output length")
	}

	output := outputBuf[:result.OutputLength]
	t.Logf("Marshaled message (%d bytes): %q", result.OutputLength, output)
	t.Logf("Body length: %d, Checksum: %d", result.BodyLength, result.Checksum)

	// Verify the output starts with "8=FIX.4.4"
	if !bytes.HasPrefix(output, []byte("8=FIX.4.4\x01")) {
		t.Error("Marshaled message doesn't start with BeginString")
	}
}

func TestMarshalMessageEmpty(t *testing.T) {
	outputBuf := make([]byte, 256)
	_, err := MarshalMessage("", []MarshalField{}, outputBuf)
	if err == nil {
		t.Error("MarshalMessage() with empty beginString should return error")
	}
}

func TestMarshalMessageSmallBuffer(t *testing.T) {
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
	}

	outputBuf := make([]byte, 10) // Too small
	result, err := MarshalMessage("FIX.4.4", fields, outputBuf)
	if err == nil {
		t.Error("MarshalMessage() with small buffer should return error")
	}
	if result != nil && result.Success {
		t.Error("MarshalMessage() with small buffer should not succeed")
	}
}

func TestFieldGetValue(t *testing.T) {
	msg := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"
	data := []byte(msg)

	result, err := ParseOne(data)
	if err != nil {
		t.Fatalf("ParseOne() error = %v", err)
	}

	if result == nil || len(result.Fields) == 0 {
		t.Fatal("ParseOne() returned no fields")
	}

	// Get the first field value (should be "FIX.4.4")
	field := &result.Fields[0]
	value := field.GetValue(data, result.MsgOffset)
	if value == nil {
		t.Fatal("GetValue() returned nil")
	}

	t.Logf("Field tag=%d, value=%q", field.Tag, value)
}

func TestMessageValidation(t *testing.T) {
	msg := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"
	data := []byte(msg)

	result, err := ParseOne(data)
	if err != nil {
		t.Fatalf("ParseOne() error = %v", err)
	}

	if result == nil {
		t.Fatal("ParseOne() returned nil result")
	}

	t.Logf("ValidateChecksum: %v", result.ValidateChecksum())
	t.Logf("ValidateBodyLength: %v", result.ValidateBodyLength())
	t.Logf("HasRequiredTags: %v", result.HasRequiredTags())
}

func TestRoundtrip(t *testing.T) {
	// Marshal a message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
	}

	outputBuf := make([]byte, 256)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, outputBuf)
	if err != nil {
		t.Fatalf("MarshalMessage() error = %v", err)
	}

	marshaled := outputBuf[:marshalResult.OutputLength]
	t.Logf("Marshaled: %q", marshaled)

	// Parse it back
	parseResult, err := ParseOne(marshaled)
	if err != nil {
		t.Fatalf("ParseOne() error = %v", err)
	}

	if parseResult == nil {
		t.Fatal("ParseOne() returned nil result")
	}

	// Validate
	if !parseResult.ValidateChecksum() {
		t.Error("Roundtrip: checksum validation failed")
	}

	if !parseResult.ValidateBodyLength() {
		t.Error("Roundtrip: body length validation failed")
	}

	t.Logf("Roundtrip successful: %d fields parsed", parseResult.FieldCount)
}

func TestParseFieldsInto(t *testing.T) {
	// Create a simple FIX message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
	}

	buffer := make([]byte, 512)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, buffer)
	if err != nil {
		t.Fatalf("MarshalMessage() error = %v", err)
	}

	data := buffer[:marshalResult.OutputLength]
	t.Logf("Test data: %q", data)

	// Parse using ParseFieldsInto (SoA format)
	result, err := ParseFieldsInto(data, MaxFieldsPerMessage)
	if err != nil {
		t.Fatalf("ParseFieldsInto() error = %v", err)
	}
	if result == nil {
		t.Fatal("ParseFieldsInto() returned nil")
	}

	t.Logf("Parsed %d fields", result.FieldCount)
	t.Logf("Message offset: %d, length: %d", result.MsgOffset, result.MsgLength)
	t.Logf("Computed checksum: %d, declared: %d", result.ComputedChecksum%256, result.DeclaredChecksum)
	t.Logf("Computed body length: %d, declared: %d", result.ComputedBodyLength, result.DeclaredBodyLength)

	if result.FieldCount == 0 {
		t.Error("Expected non-zero field count")
	}

	// Verify arrays have correct length
	if len(result.Tags) != result.FieldCount {
		t.Errorf("Tags length = %d, want %d", len(result.Tags), result.FieldCount)
	}
	if len(result.ValueOffsets) != result.FieldCount {
		t.Errorf("ValueOffsets length = %d, want %d", len(result.ValueOffsets), result.FieldCount)
	}
	if len(result.ValueLengths) != result.FieldCount {
		t.Errorf("ValueLengths length = %d, want %d", len(result.ValueLengths), result.FieldCount)
	}

	// Verify we can extract field values
	for i := 0; i < result.FieldCount; i++ {
		tag := result.Tags[i]
		offset := result.ValueOffsets[i]
		length := result.ValueLengths[i]
		value := data[offset : offset+uint32(length)]
		t.Logf("Field %d: tag=%d, value=%q", i, tag, value)
	}
}

func TestParseFieldsAoS(t *testing.T) {
	// Create a simple FIX message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
	}

	buffer := make([]byte, 512)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, buffer)
	if err != nil {
		t.Fatalf("MarshalMessage() error = %v", err)
	}

	data := buffer[:marshalResult.OutputLength]
	t.Logf("Test data: %q", data)

	// Parse using ParseFieldsAoS (AoS format)
	parsedFields, result, err := ParseFieldsAoS(data, MaxFieldsPerMessage)
	if err != nil {
		t.Fatalf("ParseFieldsAoS() error = %v", err)
	}
	if result == nil {
		t.Fatal("ParseFieldsAoS() returned nil result")
	}
	if parsedFields == nil {
		t.Fatal("ParseFieldsAoS() returned nil fields")
	}

	t.Logf("Parsed %d fields", result.FieldCount)
	t.Logf("Message offset: %d, length: %d", result.MsgOffset, result.MsgLength)

	if len(parsedFields) != result.FieldCount {
		t.Errorf("Fields length = %d, want %d", len(parsedFields), result.FieldCount)
	}

	// Verify we can extract field values
	for i, field := range parsedFields {
		offset := field.ValueOffset
		length := field.ValueLength
		value := data[offset : offset+uint32(length)]
		t.Logf("Field %d: tag=%d, value=%q, section=%d", i, field.Tag, value, field.Section)
	}
}

func TestParseFieldsComparison(t *testing.T) {
	// Create a test message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
		{Tag: 34, Value: []byte("1")},
	}

	buffer := make([]byte, 512)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, buffer)
	if err != nil {
		t.Fatalf("MarshalMessage() error = %v", err)
	}

	data := buffer[:marshalResult.OutputLength]

	// Parse with both methods
	soaResult, err := ParseFieldsInto(data, MaxFieldsPerMessage)
	if err != nil {
		t.Fatalf("ParseFieldsInto() error = %v", err)
	}

	aosFields, aosResult, err := ParseFieldsAoS(data, MaxFieldsPerMessage)
	if err != nil {
		t.Fatalf("ParseFieldsAoS() error = %v", err)
	}

	// Compare results
	if soaResult.FieldCount != aosResult.FieldCount {
		t.Errorf("Field count mismatch: SoA=%d, AoS=%d", soaResult.FieldCount, aosResult.FieldCount)
	}

	if soaResult.MsgOffset != aosResult.MsgOffset {
		t.Errorf("Message offset mismatch: SoA=%d, AoS=%d", soaResult.MsgOffset, aosResult.MsgOffset)
	}

	if soaResult.MsgLength != aosResult.MsgLength {
		t.Errorf("Message length mismatch: SoA=%d, AoS=%d", soaResult.MsgLength, aosResult.MsgLength)
	}

	// Compare field data
	for i := 0; i < soaResult.FieldCount; i++ {
		if soaResult.Tags[i] != aosFields[i].Tag {
			t.Errorf("Field %d tag mismatch: SoA=%d, AoS=%d", i, soaResult.Tags[i], aosFields[i].Tag)
		}
		if soaResult.ValueOffsets[i] != aosFields[i].ValueOffset {
			t.Errorf("Field %d value offset mismatch: SoA=%d, AoS=%d", i, soaResult.ValueOffsets[i], aosFields[i].ValueOffset)
		}
		if soaResult.ValueLengths[i] != aosFields[i].ValueLength {
			t.Errorf("Field %d value length mismatch: SoA=%d, AoS=%d", i, soaResult.ValueLengths[i], aosFields[i].ValueLength)
		}
	}

	t.Logf("Both parsing methods produced identical results for %d fields", soaResult.FieldCount)
}

// Benchmark tests

func BenchmarkChecksum(b *testing.B) {
	sizes := []struct {
		name string
		size int
	}{
		{"Small/50B", 50},
		{"Medium/150B", 150},
		{"Large/300B", 300},
		{"XLarge/1KB", 1024},
	}

	for _, sz := range sizes {
		b.Run(sz.name, func(b *testing.B) {
			data := make([]byte, sz.size)
			for i := range data {
				data[i] = byte(i % 256)
			}

			b.ResetTimer()
			b.SetBytes(int64(sz.size))
			for i := 0; i < b.N; i++ {
				_ = Checksum(data)
			}
		})
	}
}

func BenchmarkParseOne(b *testing.B) {
	messages := []struct {
		name string
		msg  string
	}{
		{
			"Small/6fields",
			"8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01",
		},
		{
			"Medium/15fields",
			"8=FIX.4.4\x019=150\x0135=D\x0149=SENDER\x0156=TARGET\x0134=1\x0152=20240101-12:00:00\x0111=ORDER123\x0121=1\x0155=AAPL\x0154=1\x0138=100\x0140=2\x0144=150.50\x0159=0\x0160=20240101-12:00:00\x0110=000\x01",
		},
		{
			"Large/25fields",
			"8=FIX.4.4\x019=300\x0135=D\x0149=SENDER\x0156=TARGET\x0134=1\x0152=20240101-12:00:00\x0111=ORDER123\x0121=1\x0155=AAPL\x0154=1\x0138=100\x0140=2\x0144=150.50\x0159=0\x0160=20240101-12:00:00\x011=ACCOUNT\x0147=A\x0163=0\x0164=20240102\x01109=CLIENT\x01110=DESK\x01111=100\x01526=ID123\x01453=1\x01448=PARTY\x01447=D\x01452=1\x0110=000\x01",
		},
	}

	for _, msg := range messages {
		b.Run(msg.name, func(b *testing.B) {
			data := []byte(msg.msg)
			b.SetBytes(int64(len(data)))

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_, _ = ParseOne(data)
			}
		})
	}
}

func BenchmarkParseMessages(b *testing.B) {
	smallMsg := "8=FIX.4.4\x019=40\x0135=D\x0149=SENDER\x0156=TARGET\x0110=000\x01"

	batches := []struct {
		name  string
		count int
	}{
		{"1msg", 1},
		{"10msg", 10},
		{"100msg", 100},
		{"1000msg", 1000},
	}

	for _, batch := range batches {
		b.Run(batch.name, func(b *testing.B) {
			// Create batch of messages
			var buf bytes.Buffer
			for i := 0; i < batch.count; i++ {
				buf.WriteString(smallMsg)
			}
			data := buf.Bytes()
			b.SetBytes(int64(len(data)))

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_, _, _ = ParseMessages(data)
			}
		})
	}
}

func BenchmarkMarshalMessage(b *testing.B) {
	tests := []struct {
		name   string
		fields []MarshalField
	}{
		{
			"Small/3fields",
			[]MarshalField{
				{Tag: 35, Value: []byte("D")},
				{Tag: 49, Value: []byte("SENDER")},
				{Tag: 56, Value: []byte("TARGET")},
			},
		},
		{
			"Medium/10fields",
			[]MarshalField{
				{Tag: 35, Value: []byte("D")},
				{Tag: 49, Value: []byte("SENDER")},
				{Tag: 56, Value: []byte("TARGET")},
				{Tag: 34, Value: []byte("1")},
				{Tag: 52, Value: []byte("20240101-12:00:00")},
				{Tag: 11, Value: []byte("ORDER123")},
				{Tag: 55, Value: []byte("AAPL")},
				{Tag: 54, Value: []byte("1")},
				{Tag: 38, Value: []byte("100")},
				{Tag: 44, Value: []byte("150.50")},
			},
		},
		{
			"Large/20fields",
			[]MarshalField{
				{Tag: 35, Value: []byte("D")},
				{Tag: 49, Value: []byte("SENDER")},
				{Tag: 56, Value: []byte("TARGET")},
				{Tag: 34, Value: []byte("1")},
				{Tag: 52, Value: []byte("20240101-12:00:00")},
				{Tag: 11, Value: []byte("ORDER123")},
				{Tag: 55, Value: []byte("AAPL")},
				{Tag: 54, Value: []byte("1")},
				{Tag: 38, Value: []byte("100")},
				{Tag: 44, Value: []byte("150.50")},
				{Tag: 1, Value: []byte("ACCOUNT")},
				{Tag: 47, Value: []byte("A")},
				{Tag: 63, Value: []byte("0")},
				{Tag: 64, Value: []byte("20240102")},
				{Tag: 109, Value: []byte("CLIENT")},
				{Tag: 110, Value: []byte("DESK")},
				{Tag: 111, Value: []byte("100")},
				{Tag: 526, Value: []byte("ID123")},
				{Tag: 453, Value: []byte("1")},
				{Tag: 448, Value: []byte("PARTY")},
			},
		},
	}

	for _, tt := range tests {
		b.Run(tt.name, func(b *testing.B) {
			outputBuf := make([]byte, 1024)

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_, _ = MarshalMessage("FIX.4.4", tt.fields, outputBuf)
			}
		})
	}
}

func BenchmarkRoundtrip(b *testing.B) {
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
		{Tag: 34, Value: []byte("1")},
		{Tag: 52, Value: []byte("20240101-12:00:00")},
		{Tag: 11, Value: []byte("ORDER123")},
	}

	outputBuf := make([]byte, 512)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		// Marshal
		result, err := MarshalMessage("FIX.4.4", fields, outputBuf)
		if err != nil {
			b.Fatal(err)
		}

		// Parse
		marshaled := outputBuf[:result.OutputLength]
		_, err = ParseOne(marshaled)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkParseFieldsInto(b *testing.B) {
	// Create test message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
		{Tag: 34, Value: []byte("1")},
		{Tag: 52, Value: []byte("20240101-12:00:00")},
	}

	buffer := make([]byte, 512)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, buffer)
	if err != nil {
		b.Fatal(err)
	}

	data := buffer[:marshalResult.OutputLength]
	b.SetBytes(int64(len(data)))

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = ParseFieldsInto(data, MaxFieldsPerMessage)
	}
}

func BenchmarkParseFieldsAoS(b *testing.B) {
	// Create test message
	fields := []MarshalField{
		{Tag: 35, Value: []byte("D")},
		{Tag: 49, Value: []byte("SENDER")},
		{Tag: 56, Value: []byte("TARGET")},
		{Tag: 34, Value: []byte("1")},
		{Tag: 52, Value: []byte("20240101-12:00:00")},
	}

	buffer := make([]byte, 512)
	marshalResult, err := MarshalMessage("FIX.4.4", fields, buffer)
	if err != nil {
		b.Fatal(err)
	}

	data := buffer[:marshalResult.OutputLength]
	b.SetBytes(int64(len(data)))

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _, _ = ParseFieldsAoS(data, MaxFieldsPerMessage)
	}
}
