package codec

/*
#include <stdlib.h>
#include <string.h>
#include <fin-kit/codec/fix_codec.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// FIX protocol constants
const (
	MaxFieldsPerMessage = 256  // Maximum fields per message
	MaxMessages         = 16   // Maximum messages per batch parse
	SOH                 = 0x01 // FIX field separator
	SingleMaxFields     = 128  // Maximum fields for single message parse
)

// Section represents a FIX message section
type Section uint8

const (
	SectionHeader  Section = 0 // Message header
	SectionBody    Section = 1 // Message body
	SectionTrailer Section = 2 // Message trailer
)

// Required tag bitmap constants
const (
	ReqBitBeginString  = 1 << 0 // tag 8  - BeginString
	ReqBitBodyLength   = 1 << 1 // tag 9  - BodyLength
	ReqBitMsgType      = 1 << 2 // tag 35 - MsgType
	ReqBitMsgSeqNum    = 1 << 3 // tag 34 - MsgSeqNum
	ReqBitSenderCompID = 1 << 4 // tag 49 - SenderCompID
	ReqBitTargetCompID = 1 << 5 // tag 56 - TargetCompID
	ReqBitSendingTime  = 1 << 6 // tag 52 - SendingTime
	ReqBitChecksum     = 1 << 7 // tag 10 - CheckSum
	ReqBitAllBasic     = 0xFF   // All basic required tags
)

// Field represents a parsed FIX field
type Field struct {
	Tag         int32   // Tag number (e.g., 8, 35, 49)
	ValueOffset uint32  // Value start offset (after '=')
	ValueLength uint16  // Value byte length (excluding SOH)
	RawOffset   uint32  // Raw field start offset ("tag=value\x01")
	RawLength   uint16  // Raw field byte length (including SOH)
	Section     Section // Section: Header/Body/Trailer
}

// Message represents a parsed FIX message
type Message struct {
	MsgOffset          uint32  // Message start offset in input buffer
	MsgLength          uint32  // Total message byte count
	ComputedChecksum   uint32  // C-computed raw byte sum
	ComputedBodyLength uint32  // C-computed BodyLength
	DeclaredChecksum   uint32  // Declared CheckSum value from tag 10
	DeclaredBodyLength uint32  // Declared BodyLength value from tag 9
	FieldCount         int32   // Total field count
	HeaderFieldCount   int32   // Header field count
	BodyFieldCount     int32   // Body field count
	TrailerFieldCount  int32   // Trailer field count
	RequiredTagsBitmap uint32  // Required tag presence bitmap
	Fields             []Field // All fields
}

// ParseResult represents the result of parsing FIX messages
type ParseResult struct {
	MsgCount      int32     // Number of successfully parsed messages
	ConsumedBytes int32     // Total bytes consumed from input buffer
	Messages      []Message // Parse results for each message
}

// MarshalField represents a field for marshaling
type MarshalField struct {
	Tag   int32  // Tag number
	Value []byte // Value bytes
}

// MarshalResult represents the result of marshaling a FIX message
type MarshalResult struct {
	Success      bool   // True if marshaling succeeded
	OutputLength uint32 // Actual bytes written to output buffer
	BodyLength   uint32 // Calculated BodyLength
	Checksum     uint32 // Calculated CheckSum (already %256)
}

// ParseMessages parses FIX messages from raw byte stream (batch mode)
//
// Automatically detects message boundaries and completes field splitting,
// tag classification, and CheckSum/BodyLength calculation in a single pass.
//
// Returns the number of successfully parsed messages and the parse result.
func ParseMessages(data []byte) (int, *ParseResult, error) {
	if len(data) == 0 {
		return 0, nil, fmt.Errorf("input data is empty")
	}

	var cResult C.fc_fix_parse_result_t
	count := C.fc_fix_parse_messages(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		&cResult,
	)

	if count == 0 {
		return 0, nil, nil
	}

	result := &ParseResult{
		MsgCount:      int32(cResult.msg_count),
		ConsumedBytes: int32(cResult.consumed_bytes),
		Messages:      make([]Message, int(cResult.msg_count)),
	}

	for i := 0; i < int(cResult.msg_count); i++ {
		cMsg := &cResult.messages[i]
		msg := &result.Messages[i]

		msg.MsgOffset = uint32(cMsg.msg_offset)
		msg.MsgLength = uint32(cMsg.msg_length)
		msg.ComputedChecksum = uint32(cMsg.computed_checksum)
		msg.ComputedBodyLength = uint32(cMsg.computed_body_length)
		msg.DeclaredChecksum = uint32(cMsg.declared_checksum)
		msg.DeclaredBodyLength = uint32(cMsg.declared_body_length)
		msg.FieldCount = int32(cMsg.field_count)
		msg.HeaderFieldCount = int32(cMsg.header_field_count)
		msg.BodyFieldCount = int32(cMsg.body_field_count)
		msg.TrailerFieldCount = int32(cMsg.trailer_field_count)
		msg.RequiredTagsBitmap = uint32(cMsg.required_tags_bitmap)

		msg.Fields = make([]Field, int(cMsg.field_count))
		for j := 0; j < int(cMsg.field_count); j++ {
			cField := &cMsg.fields[j]
			msg.Fields[j] = Field{
				Tag:         int32(cField.tag),
				ValueOffset: uint32(cField.value_offset),
				ValueLength: uint16(cField.value_length),
				RawOffset:   uint32(cField.raw_offset),
				RawLength:   uint16(cField.raw_length),
				Section:     Section(cField.section),
			}
		}
	}

	return int(count), result, nil
}

// ParseOne parses a single FIX message (lightweight version)
//
// Same functionality as ParseMessages, but only parses the first complete
// message, using a smaller result structure to reduce memory allocation overhead.
func ParseOne(data []byte) (*Message, error) {
	if len(data) == 0 {
		return nil, fmt.Errorf("input data is empty")
	}

	var cResult C.fc_fix_parse_single_result_t
	success := C.fc_fix_parse_one(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		&cResult,
	)

	if success == 0 {
		return nil, nil
	}

	msg := &Message{
		MsgOffset:          uint32(cResult.msg_offset),
		MsgLength:          uint32(cResult.msg_length),
		ComputedChecksum:   uint32(cResult.computed_checksum),
		ComputedBodyLength: uint32(cResult.computed_body_length),
		DeclaredChecksum:   uint32(cResult.declared_checksum),
		DeclaredBodyLength: uint32(cResult.declared_body_length),
		FieldCount:         int32(cResult.field_count),
		HeaderFieldCount:   int32(cResult.header_field_count),
		BodyFieldCount:     int32(cResult.body_field_count),
		TrailerFieldCount:  int32(cResult.trailer_field_count),
		RequiredTagsBitmap: uint32(cResult.required_tags_bitmap),
		Fields:             make([]Field, int(cResult.field_count)),
	}

	for i := 0; i < int(cResult.field_count); i++ {
		cField := &cResult.fields[i]
		msg.Fields[i] = Field{
			Tag:         int32(cField.tag),
			ValueOffset: uint32(cField.value_offset),
			ValueLength: uint16(cField.value_length),
			RawOffset:   uint32(cField.raw_offset),
			RawLength:   uint16(cField.raw_length),
			Section:     Section(cField.section),
		}
	}

	return msg, nil
}

// MarshalMessage assembles structured fields into a complete FIX message
//
// Automatically calculates and fills in BodyLength (tag 9) and CheckSum (tag 10).
// The fields slice should contain all fields except tags 8, 9, and 10.
func MarshalMessage(beginString string, fields []MarshalField, outputBuf []byte) (*MarshalResult, error) {
	if len(beginString) == 0 {
		return nil, fmt.Errorf("beginString is empty")
	}
	if len(outputBuf) == 0 {
		return nil, fmt.Errorf("output buffer is empty")
	}

	// Allocate C memory for fields to avoid CGO pointer issues
	cFields := (*C.fc_fix_marshal_field_t)(C.malloc(C.size_t(len(fields)) * C.size_t(unsafe.Sizeof(C.fc_fix_marshal_field_t{}))))
	if cFields == nil && len(fields) > 0 {
		return nil, fmt.Errorf("failed to allocate C memory for fields")
	}
	defer C.free(unsafe.Pointer(cFields))

	// Allocate C memory for all field values
	valuePtrs := make([]unsafe.Pointer, len(fields))
	for i, field := range fields {
		fieldPtr := (*C.fc_fix_marshal_field_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cFields)) + uintptr(i)*unsafe.Sizeof(C.fc_fix_marshal_field_t{})))
		fieldPtr.tag = C.int32_t(field.Tag)

		if len(field.Value) > 0 {
			// Allocate C memory for this field's value
			valuePtr := C.malloc(C.size_t(len(field.Value)))
			if valuePtr == nil {
				// Clean up previously allocated values
				for j := 0; j < i; j++ {
					if valuePtrs[j] != nil {
						C.free(valuePtrs[j])
					}
				}
				return nil, fmt.Errorf("failed to allocate C memory for field value")
			}
			valuePtrs[i] = valuePtr

			// Copy value to C memory
			C.memcpy(valuePtr, unsafe.Pointer(&field.Value[0]), C.size_t(len(field.Value)))

			fieldPtr.value = (*C.uint8_t)(valuePtr)
			fieldPtr.value_length = C.uint16_t(len(field.Value))
		} else {
			fieldPtr.value = nil
			fieldPtr.value_length = 0
		}
	}

	// Clean up value memory after the call
	defer func() {
		for _, ptr := range valuePtrs {
			if ptr != nil {
				C.free(ptr)
			}
		}
	}()

	beginStringBytes := []byte(beginString)
	cResult := C.fc_fix_marshal_message(
		(*C.uint8_t)(unsafe.Pointer(&beginStringBytes[0])),
		C.int32_t(len(beginString)),
		cFields,
		C.int32_t(len(fields)),
		(*C.uint8_t)(unsafe.Pointer(&outputBuf[0])),
		C.size_t(len(outputBuf)),
	)

	result := &MarshalResult{
		Success:      cResult.success != 0,
		OutputLength: uint32(cResult.output_length),
		BodyLength:   uint32(cResult.body_length),
		Checksum:     uint32(cResult.checksum),
	}

	if !result.Success {
		return result, fmt.Errorf("marshal failed: buffer too small or invalid input")
	}

	return result, nil
}

// Checksum calculates the FIX checksum for the given data
//
// Returns the raw byte sum. Caller should apply %256 to get the final checksum.
func Checksum(data []byte) uint32 {
	if len(data) == 0 {
		return 0
	}

	sum := C.fc_fix_checksum(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
	)

	return uint32(sum)
}

// SIMDLevel returns the current SIMD acceleration level
//
// Returns one of: "AVX2", "SSE4.2", "Scalar (no SIMD)", or "Unknown"
func SIMDLevel() string {
	cStr := C.fc_fix_simd_level()
	return C.GoString(cStr)
}

// GetFieldValue extracts the field value from the original message data
//
// This is a helper function to extract field values using zero-copy slicing.
func (f *Field) GetValue(msgData []byte, msgOffset uint32) []byte {
	start := msgOffset + f.ValueOffset
	end := start + uint32(f.ValueLength)
	if end > uint32(len(msgData)) {
		return nil
	}
	return msgData[start:end]
}

// GetRawField extracts the raw field (tag=value\x01) from the original message data
func (f *Field) GetRawField(msgData []byte, msgOffset uint32) []byte {
	start := msgOffset + f.RawOffset
	end := start + uint32(f.RawLength)
	if end > uint32(len(msgData)) {
		return nil
	}
	return msgData[start:end]
}

// ValidateChecksum validates the message checksum
func (m *Message) ValidateChecksum() bool {
	return (m.ComputedChecksum % 256) == (m.DeclaredChecksum % 256)
}

// ValidateBodyLength validates the message body length
func (m *Message) ValidateBodyLength() bool {
	return m.ComputedBodyLength == m.DeclaredBodyLength
}

// HasRequiredTags checks if all required tags are present
func (m *Message) HasRequiredTags() bool {
	return (m.RequiredTagsBitmap & ReqBitAllBasic) == ReqBitAllBasic
}

// GetMessageData extracts the complete message from the original data
func (m *Message) GetMessageData(data []byte) []byte {
	start := m.MsgOffset
	end := start + m.MsgLength
	if end > uint32(len(data)) {
		return nil
	}
	return data[start:end]
}

// ParseFieldsResult represents the result of parsing fields in SoA format
type ParseFieldsResult struct {
	FieldCount         int      // Number of fields parsed
	MsgOffset          uint32   // Message start offset
	MsgLength          uint32   // Message length
	ComputedChecksum   uint32   // Computed checksum
	ComputedBodyLength uint32   // Computed body length
	DeclaredChecksum   uint32   // Declared checksum
	DeclaredBodyLength uint32   // Declared body length
	Tags               []int32  // Field tags
	ValueOffsets       []uint32 // Field value offsets
	ValueLengths       []uint16 // Field value lengths
	RawOffsets         []uint32 // Field raw offsets
	RawLengths         []uint16 // Field raw lengths
	Sections           []uint8  // Field sections
}

// ParseFieldsInto parses FIX message fields into separate arrays (SoA format)
//
// This is a lower-level API that provides Structure-of-Arrays layout,
// which may be more efficient for certain processing patterns.
func ParseFieldsInto(data []byte, maxFields int) (*ParseFieldsResult, error) {
	if len(data) == 0 {
		return nil, fmt.Errorf("input data is empty")
	}
	if maxFields <= 0 {
		maxFields = MaxFieldsPerMessage
	}

	result := &ParseFieldsResult{
		Tags:         make([]int32, maxFields),
		ValueOffsets: make([]uint32, maxFields),
		ValueLengths: make([]uint16, maxFields),
		RawOffsets:   make([]uint32, maxFields),
		RawLengths:   make([]uint16, maxFields),
		Sections:     make([]uint8, maxFields),
	}

	var msgOffset, msgLength, checksum, bodyLength, declaredChecksum, declaredBodyLength C.uint32_t

	count := C.fc_fix_parse_fields_into(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		(*C.int32_t)(unsafe.Pointer(&result.Tags[0])),
		(*C.uint32_t)(unsafe.Pointer(&result.ValueOffsets[0])),
		(*C.uint16_t)(unsafe.Pointer(&result.ValueLengths[0])),
		(*C.uint32_t)(unsafe.Pointer(&result.RawOffsets[0])),
		(*C.uint16_t)(unsafe.Pointer(&result.RawLengths[0])),
		(*C.uint8_t)(unsafe.Pointer(&result.Sections[0])),
		C.int(maxFields),
		&msgOffset,
		&msgLength,
		&checksum,
		&bodyLength,
		&declaredChecksum,
		&declaredBodyLength,
	)

	if count == 0 {
		return nil, nil
	}

	result.FieldCount = int(count)
	result.MsgOffset = uint32(msgOffset)
	result.MsgLength = uint32(msgLength)
	result.ComputedChecksum = uint32(checksum)
	result.ComputedBodyLength = uint32(bodyLength)
	result.DeclaredChecksum = uint32(declaredChecksum)
	result.DeclaredBodyLength = uint32(declaredBodyLength)

	// Trim slices to actual field count
	result.Tags = result.Tags[:count]
	result.ValueOffsets = result.ValueOffsets[:count]
	result.ValueLengths = result.ValueLengths[:count]
	result.RawOffsets = result.RawOffsets[:count]
	result.RawLengths = result.RawLengths[:count]
	result.Sections = result.Sections[:count]

	return result, nil
}

// ParseFieldsAoS parses FIX message fields into an array of structures (AoS format)
//
// This is a lower-level API that provides Array-of-Structures layout.
// Returns the parsed fields and message metadata.
func ParseFieldsAoS(data []byte, maxFields int) ([]Field, *ParseFieldsResult, error) {
	if len(data) == 0 {
		return nil, nil, fmt.Errorf("input data is empty")
	}
	if maxFields <= 0 {
		maxFields = MaxFieldsPerMessage
	}

	// Allocate C memory for fields array
	cFields := (*C.fc_fix_parsed_field_t)(C.malloc(C.size_t(maxFields) * C.size_t(unsafe.Sizeof(C.fc_fix_parsed_field_t{}))))
	if cFields == nil {
		return nil, nil, fmt.Errorf("failed to allocate C memory for fields")
	}
	defer C.free(unsafe.Pointer(cFields))

	var msgOffset, msgLength, checksum, bodyLength, declaredChecksum, declaredBodyLength C.uint32_t

	count := C.fc_fix_parse_fields_aos(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		cFields,
		C.int(maxFields),
		&msgOffset,
		&msgLength,
		&checksum,
		&bodyLength,
		&declaredChecksum,
		&declaredBodyLength,
	)

	if count == 0 {
		return nil, nil, nil
	}

	// Convert C fields to Go fields
	fields := make([]Field, int(count))
	for i := 0; i < int(count); i++ {
		cField := (*C.fc_fix_parsed_field_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cFields)) + uintptr(i)*unsafe.Sizeof(C.fc_fix_parsed_field_t{})))
		fields[i] = Field{
			Tag:         int32(cField.tag),
			ValueOffset: uint32(cField.value_offset),
			ValueLength: uint16(cField.value_length),
			RawOffset:   uint32(cField.raw_offset),
			RawLength:   uint16(cField.raw_length),
			Section:     Section(cField.section),
		}
	}

	result := &ParseFieldsResult{
		FieldCount:         int(count),
		MsgOffset:          uint32(msgOffset),
		MsgLength:          uint32(msgLength),
		ComputedChecksum:   uint32(checksum),
		ComputedBodyLength: uint32(bodyLength),
		DeclaredChecksum:   uint32(declaredChecksum),
		DeclaredBodyLength: uint32(declaredBodyLength),
	}

	return fields, result, nil
}
