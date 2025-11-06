#pragma once

#include "core/types.hpp"
#include "ppu/ppu_registers.hpp"
#include <array>

namespace nes {

/// PPU Memory management - handles VRAM and palette memory
/// Note: OAM (Object Attribute Memory) is handled directly by the PPU class
class PPUMemory {
  public:
	PPUMemory();
	~PPUMemory() = default;

	// Memory access methods
	uint8_t read_vram(uint16_t address);
	void write_vram(uint16_t address, uint8_t value);

	uint8_t read_palette(uint8_t index);
	void write_palette(uint8_t index, uint8_t value);

	// Reset memory to power-on state
	void reset();
	void power_on();

	// Get raw memory arrays (for debugging/direct access)
	const std::array<uint8_t, 2048> &get_vram() const {
		return vram_;
	}
	const std::array<uint8_t, 32> &get_palette_ram() const {
		return palette_ram_;
	}

	// Pattern table access (reads from cartridge CHR ROM/RAM)
	uint8_t read_pattern_table(uint16_t address) const;
	void write_pattern_table(uint16_t address, uint8_t value);

	// Nametable mirroring support
	void set_mirroring_mode(bool vertical_mirroring);

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset);

  private:
	// Internal memory arrays
	std::array<uint8_t, 2048> vram_;			   // 2KB VRAM (nametables)
	std::array<uint8_t, 32> palette_ram_;		   // 32 bytes palette RAM
	std::array<uint8_t, 8192> chr_ram_fallback_{}; // 8KB CHR RAM for tests/no-mapper

	// Mirroring state
	bool vertical_mirroring_;

	// Address mapping helpers
	uint16_t map_nametable_address(uint16_t address);
	uint8_t map_palette_address(uint8_t address);
};

/// PPU Address space constants
namespace PPUMemoryMap {
// Pattern Tables (CHR ROM/RAM)
constexpr uint16_t PATTERN_TABLE_0_START = 0x0000;
constexpr uint16_t PATTERN_TABLE_0_END = 0x0FFF;
constexpr uint16_t PATTERN_TABLE_1_START = 0x1000;
constexpr uint16_t PATTERN_TABLE_1_END = 0x1FFF;

// Nametables
constexpr uint16_t NAMETABLE_0_START = 0x2000;
constexpr uint16_t NAMETABLE_0_END = 0x23FF;
constexpr uint16_t NAMETABLE_1_START = 0x2400;
constexpr uint16_t NAMETABLE_1_END = 0x27FF;
constexpr uint16_t NAMETABLE_2_START = 0x2800;
constexpr uint16_t NAMETABLE_2_END = 0x2BFF;
constexpr uint16_t NAMETABLE_3_START = 0x2C00;
constexpr uint16_t NAMETABLE_3_END = 0x2FFF;

// Nametable mirrors
constexpr uint16_t NAMETABLE_MIRROR_START = 0x3000;
constexpr uint16_t NAMETABLE_MIRROR_END = 0x3EFF;

// Palette RAM
constexpr uint16_t PALETTE_START = 0x3F00;
constexpr uint16_t PALETTE_END = 0x3F1F;
constexpr uint16_t PALETTE_MIRROR_START = 0x3F20;
constexpr uint16_t PALETTE_MIRROR_END = 0x3FFF;

// Sizes
constexpr size_t NAMETABLE_SIZE = 0x400;	  // 1KB per nametable
constexpr size_t PATTERN_TABLE_SIZE = 0x1000; // 4KB per pattern table
constexpr size_t PALETTE_SIZE = 32;			  // 32 bytes total
constexpr size_t OAM_SIZE = 256;			  // 256 bytes total
constexpr size_t SPRITE_COUNT = 64;			  // 64 sprites total (4 bytes each)
} // namespace PPUMemoryMap

} // namespace nes
