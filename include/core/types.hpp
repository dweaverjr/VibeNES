#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <string>

namespace nes {

// =============================================================================
// Basic Types
// =============================================================================

/// Strong type for memory addresses (16-bit for 6502)
using Address = std::uint16_t;

/// Strong type for 8-bit data values
using Byte = std::uint8_t;

/// Strong type for 16-bit data values
using Word = std::uint16_t;

/// Strong type for signed 8-bit values (relative addressing, etc.)
using SignedByte = std::int8_t;

// =============================================================================
// Timing Types
// =============================================================================

/// NES CPU master clock frequency (NTSC): 21.477272 MHz
/// CPU clock: 21.477272 MHz / 12 = 1.789773 MHz
/// PPU clock: 21.477272 MHz / 4 = 5.369318 MHz (3x CPU clock)
/// APU clock: CPU clock / 2 = 894886.5 Hz (channels clocked at half CPU rate)
constexpr std::uint64_t MASTER_CLOCK_NTSC = 21'477'272;
constexpr std::uint64_t CPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 12;
constexpr std::uint64_t PPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 4;
constexpr double APU_CLOCK_NTSC = static_cast<double>(CPU_CLOCK_NTSC) / 2.0; // 894886.5 Hz

/// Strong type for CPU cycles using std::chrono for precision
using CpuCycle = std::chrono::duration<std::int64_t, std::ratio<1, CPU_CLOCK_NTSC>>;

/// Strong type for PPU dots (3 PPU dots per CPU cycle)
using PpuDot = std::chrono::duration<std::int64_t, std::ratio<1, PPU_CLOCK_NTSC>>;

/// Conversion helpers
constexpr CpuCycle cpu_cycles(std::int64_t count) noexcept {
	return CpuCycle{count};
}

constexpr PpuDot ppu_dots(std::int64_t count) noexcept {
	return PpuDot{count};
}

// =============================================================================
// Memory Constants
// =============================================================================

/// NES memory layout constants
constexpr Address RAM_START = 0x0000;
constexpr Address RAM_END = 0x07FF;
constexpr Address RAM_SIZE = 0x0800; // 2KB

constexpr Address PPU_REGISTERS_START = 0x2000;
constexpr Address PPU_REGISTERS_END = 0x2007;

constexpr Address APU_IO_START = 0x4000;
constexpr Address APU_IO_END = 0x4017;

constexpr Address CARTRIDGE_START = 0x4020;
constexpr Address CARTRIDGE_END = 0xFFFF;

/// PPU memory constants
constexpr Address CHR_PATTERN_0_START = 0x0000;
constexpr Address CHR_PATTERN_1_START = 0x1000;
constexpr Address NAMETABLE_START = 0x2000;
constexpr Address PALETTE_START = 0x3F00;

constexpr std::size_t OAM_SIZE = 256;	// Object Attribute Memory
constexpr std::size_t VRAM_SIZE = 2048; // Video RAM

// =============================================================================
// Register Types
// =============================================================================

/// CPU register enumeration
enum class CpuRegister : std::uint8_t {
	A,	// Accumulator
	X,	// Index register X
	Y,	// Index register Y
	SP, // Stack pointer
	PC, // Program counter (16-bit)
	P	// Processor status
};

/// CPU status flag bits
enum class StatusFlag : std::uint8_t {
	CARRY = 0x01,
	ZERO = 0x02,
	INTERRUPT = 0x04,
	DECIMAL = 0x08,
	BREAK = 0x10,
	UNUSED = 0x20,
	OVERFLOW = 0x40,
	NEGATIVE = 0x80
};

// =============================================================================
// Error Handling
// =============================================================================

/// Error types for emulation operations
enum class EmulationError {
	INVALID_ADDRESS,
	INVALID_OPCODE,
	STACK_OVERFLOW,
	STACK_UNDERFLOW,
	ROM_LOAD_FAILED,
	INVALID_MAPPER,
	HARDWARE_FAULT
};

/// Result type for operations that can fail
template <typename T> using EmulationResult = std::expected<T, EmulationError>;

// =============================================================================
// Concepts
// =============================================================================

/// Concept for types that can be clocked (advanced by cycles)
template <typename T>
concept Clockable = requires(T t, CpuCycle cycles) {
	{ t.tick(cycles) } -> std::same_as<void>;
};

/// Concept for memory-readable devices
template <typename T>
concept Readable = requires(T t, Address addr) {
	{ t.read(addr) } -> std::same_as<Byte>;
};

/// Concept for memory-writable devices
template <typename T>
concept Writable = requires(T t, Address addr, Byte value) {
	{ t.write(addr, value) } -> std::same_as<void>;
};

/// Concept for memory-mapped devices (both readable and writable)
template <typename T>
concept MemoryMapped = Readable<T> && Writable<T>;

/// Concept for resettable components
template <typename T>
concept Resettable = requires(T t) {
	{ t.reset() } -> std::same_as<void>;
	{ t.power_on() } -> std::same_as<void>;
};

/// Concept for emulation components (clockable and resettable)
template <typename T>
concept EmulationComponent = Clockable<T> && Resettable<T>;

// =============================================================================
// Utility Functions
// =============================================================================

/// Convert CPU cycles to PPU dots
constexpr PpuDot to_ppu_dots(CpuCycle cycles) noexcept {
	return PpuDot{cycles.count() * 3};
}

/// Convert PPU dots to CPU cycles (rounded down)
constexpr CpuCycle to_cpu_cycles(PpuDot dots) noexcept {
	return CpuCycle{dots.count() / 3};
}

/// Check if an address is in RAM range
constexpr bool is_ram_address(Address addr) noexcept {
	return addr <= RAM_END;
}

/// Check if an address is a PPU register
constexpr bool is_ppu_register(Address addr) noexcept {
	return addr >= PPU_REGISTERS_START && addr <= PPU_REGISTERS_END;
}

/// Check if an address is in APU/IO range
constexpr bool is_apu_io_address(Address addr) noexcept {
	return addr >= APU_IO_START && addr <= APU_IO_END;
}

/// Check if an address is in cartridge space
constexpr bool is_cartridge_address(Address addr) noexcept {
	return addr >= CARTRIDGE_START;
}

/// Mirror RAM address (RAM is mirrored every 2KB up to $2000)
constexpr Address mirror_ram_address(Address addr) noexcept {
	if (addr < PPU_REGISTERS_START) {
		return addr & (RAM_SIZE - 1); // Mask to 2KB boundary
	}
	return addr;
}

/// Combine two bytes into a word (little-endian)
constexpr Word make_word(Byte low, Byte high) noexcept {
	return static_cast<Word>(low) | (static_cast<Word>(high) << 8);
}

/// Extract low byte from word
constexpr Byte low_byte(Word word) noexcept {
	return static_cast<Byte>(word & 0xFF);
}

/// Extract high byte from word
constexpr Byte high_byte(Word word) noexcept {
	return static_cast<Byte>((word >> 8) & 0xFF);
}

} // namespace nes
