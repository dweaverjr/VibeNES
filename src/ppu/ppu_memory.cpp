#include "ppu/ppu_memory.hpp"
#include <cstring>

namespace nes {

PPUMemory::PPUMemory() : vertical_mirroring_(false) {
	power_on();
}

void PPUMemory::power_on() {
	// Clear all memory to 0 on power-on
	vram_.fill(0);
	palette_ram_.fill(0);
	oam_.fill(0);
	vertical_mirroring_ = false;
}

void PPUMemory::reset() {
	// Reset doesn't clear memory, only some state
	// OAM and VRAM contents are preserved through reset
}

uint8_t PPUMemory::read_vram(uint16_t address) {
	// Map the address through nametable mirroring
	uint16_t mapped_address = map_nametable_address(address);

	if (mapped_address < vram_.size()) {
		return vram_[mapped_address];
	}

	// Invalid address, return 0
	return 0;
}

void PPUMemory::write_vram(uint16_t address, uint8_t value) {
	// Map the address through nametable mirroring
	uint16_t mapped_address = map_nametable_address(address);

	if (mapped_address < vram_.size()) {
		vram_[mapped_address] = value;
	}
}

uint8_t PPUMemory::read_palette(uint8_t index) {
	// Handle palette mirroring
	uint8_t mapped_index = map_palette_address(index);

	if (mapped_index < palette_ram_.size()) {
		return palette_ram_[mapped_index];
	}

	return 0;
}

void PPUMemory::write_palette(uint8_t index, uint8_t value) {
	// Handle palette mirroring
	uint8_t mapped_index = map_palette_address(index);

	if (mapped_index < palette_ram_.size()) {
		// NES palette RAM is only 6 bits - mask out the upper 2 bits
		palette_ram_[mapped_index] = value & 0x3F;
	}
}

uint8_t PPUMemory::read_oam(uint8_t index) {
	return oam_[index];
}

void PPUMemory::write_oam(uint8_t index, uint8_t value) {
	oam_[index] = value;
}

uint8_t PPUMemory::read_pattern_table(uint16_t address) {
	// Pattern table reads come from cartridge CHR ROM/RAM
	// This is a placeholder - will be connected to cartridge later
	(void)address; // Suppress unused parameter warning
	return 0;
}

void PPUMemory::set_mirroring_mode(bool vertical_mirroring) {
	vertical_mirroring_ = vertical_mirroring;
}

uint16_t PPUMemory::map_nametable_address(uint16_t address) {
	// Remove the base nametable address to get offset
	uint16_t offset = address - PPUMemoryMap::NAMETABLE_0_START;

	// Handle mirroring based on cartridge mirroring mode
	// NES has only 2KB VRAM which holds 2 nametables
	if (vertical_mirroring_) {
		// Vertical mirroring: nametables 0 and 2 share memory, 1 and 3 share memory
		// NT0 ($2000-$23FF) -> VRAM $0000-$03FF
		// NT1 ($2400-$27FF) -> VRAM $0400-$07FF
		// NT2 ($2800-$2BFF) -> VRAM $0000-$03FF (mirror of NT0)
		// NT3 ($2C00-$2FFF) -> VRAM $0400-$07FF (mirror of NT1)
		if (offset >= 0x800) {
			offset -= 0x800; // Mirror nametables 2,3 to 0,1
		}
	} else {
		// Horizontal mirroring: nametables 0 and 1 share memory, 2 and 3 share memory
		// NT0 ($2000-$23FF) -> VRAM $0000-$03FF
		// NT1 ($2400-$27FF) -> VRAM $0000-$03FF (mirror of NT0)
		// NT2 ($2800-$2BFF) -> VRAM $0400-$07FF
		// NT3 ($2C00-$2FFF) -> VRAM $0400-$07FF (mirror of NT2)
		if (offset >= 0x800) {
			offset -= 0x400; // Map nametables 2,3 to upper 1KB
		} else if (offset >= 0x400) {
			offset -= 0x400; // Map nametable 1 to lower 1KB (mirror of NT0)
		}
	}

	// Ensure we stay within VRAM bounds
	return offset % vram_.size();
}

uint8_t PPUMemory::map_palette_address(uint8_t address) {
	// Handle palette mirroring
	uint8_t index = address & 0x1F; // Palette RAM is only 32 bytes

	// Handle background/sprite palette mirroring
	// Addresses $3F10, $3F14, $3F18, $3F1C are mirrors of $3F00, $3F04, $3F08, $3F0C
	if (index >= 0x10 && (index & 0x03) == 0) {
		index &= 0x0F; // Mirror sprite palette universal colors to background
	}

	return index;
}

} // namespace nes
