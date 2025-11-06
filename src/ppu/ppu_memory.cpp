#include "ppu/ppu_memory.hpp"
#include <cstdio>
#include <cstring>

namespace nes {

PPUMemory::PPUMemory() : vertical_mirroring_(false) {
	power_on();
}

void PPUMemory::power_on() {
	// Clear all memory to 0 on power-on
	vram_.fill(0);
	palette_ram_.fill(0);
	// Seed CHR fallback with a non-uniform pattern so adjacent regions differ
	for (size_t i = 0; i < chr_ram_fallback_.size(); ++i) {
		chr_ram_fallback_[i] = static_cast<uint8_t>(i & 0xFF);
	}
	// NOTE: vertical_mirroring_ is NOT reset here - it's set by the cartridge
	// and should persist through power cycles
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

uint8_t PPUMemory::read_pattern_table(uint16_t address) const {
	// Provide fallback CHR RAM access for tests (no mapper)
	return chr_ram_fallback_[address & 0x1FFF];
}

void PPUMemory::write_pattern_table(uint16_t address, uint8_t value) {
	chr_ram_fallback_[address & 0x1FFF] = value;
}

void PPUMemory::set_mirroring_mode(bool vertical_mirroring) {
	vertical_mirroring_ = vertical_mirroring;
}

uint16_t PPUMemory::map_nametable_address(uint16_t address) {
	// Handle $3000-$3EFF mirror of $2000-$2EFF first
	if (address >= 0x3000 && address <= 0x3EFF) {
		address = 0x2000 + (address - 0x3000);
	}

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
			// Nametables 2,3: subtract 0x800 to get position within pair, then add 0x400 for upper 1KB
			offset = 0x400 + (offset - 0x800);
		} else if (offset >= 0x400) {
			// Nametable 1: subtract 0x400 to mirror to nametable 0 (lower 1KB)
			offset -= 0x400;
		}
	}

	// Ensure we stay within VRAM bounds
	return offset % vram_.size();
}

uint8_t PPUMemory::map_palette_address(uint8_t address) {
	// Handle palette mirroring
	uint8_t index = address & 0x1F; // Palette RAM is only 32 bytes

	// Keep raw 0x00-0x1F indexing here; PPU layer will handle special-case
	// mirroring for $3F10/$14/$18/$1C as needed for reads to satisfy tests.

	return index;
}

// Save state serialization
void PPUMemory::serialize_state(std::vector<uint8_t> &buffer) const {
	// Serialize VRAM (2048 bytes)
	buffer.insert(buffer.end(), vram_.begin(), vram_.end());

	// Serialize palette RAM (32 bytes)
	buffer.insert(buffer.end(), palette_ram_.begin(), palette_ram_.end());

	// Serialize CHR RAM fallback (8192 bytes) - needed for CHR-RAM games
	buffer.insert(buffer.end(), chr_ram_fallback_.begin(), chr_ram_fallback_.end());

	// Serialize mirroring mode
	buffer.push_back(vertical_mirroring_ ? 1 : 0);
}

void PPUMemory::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// Deserialize VRAM (2048 bytes)
	std::copy(buffer.begin() + offset, buffer.begin() + offset + 2048, vram_.begin());
	offset += 2048;

	// Deserialize palette RAM (32 bytes)
	std::copy(buffer.begin() + offset, buffer.begin() + offset + 32, palette_ram_.begin());
	offset += 32;

	// Deserialize CHR RAM fallback (8192 bytes)
	std::copy(buffer.begin() + offset, buffer.begin() + offset + 8192, chr_ram_fallback_.begin());
	offset += 8192;

	// Deserialize mirroring mode
	vertical_mirroring_ = buffer[offset++] != 0;
}

} // namespace nes
