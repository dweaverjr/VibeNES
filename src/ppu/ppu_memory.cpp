#include "ppu/ppu_memory.hpp"
#include "cartridge/cartridge.hpp"
#include <cstdio>
#include <cstring>

namespace nes {

PPUMemory::PPUMemory() : vertical_mirroring_(false), cartridge_(nullptr) {
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

void PPUMemory::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
	cartridge_ = cartridge;
}

bool PPUMemory::get_vertical_mirroring() const {
	// If cartridge is connected, get mirroring from it (supports dynamic mirroring)
	if (cartridge_) {
		auto mirroring = cartridge_->get_mirroring();
		return (mirroring == Mapper::Mirroring::Vertical);
	}
	// Fallback to static mirroring mode (for tests without cartridge)
	return vertical_mirroring_;
}

uint16_t PPUMemory::map_nametable_address(uint16_t address) {
	// Handle $3000-$3EFF mirror of $2000-$2EFF first
	if (address >= 0x3000 && address <= 0x3EFF) {
		address = 0x2000 + (address - 0x3000);
	}

	// Remove the base nametable address to get offset within the 4-nametable space
	uint16_t offset = address - PPUMemoryMap::NAMETABLE_0_START;
	// Which logical nametable (0-3) and offset within it
	uint16_t nt_index = (offset >> 10) & 0x03; // 0-3
	uint16_t nt_offset = offset & 0x03FF;	   // 0-$3FF within nametable

	// Get current mirroring mode from cartridge (supports dynamic mirroring)
	// MMC1 can switch between all 4 modes at runtime (single-screen low/high,
	// vertical, horizontal).  Previous code only handled vertical/horizontal,
	// causing wrong nametable data for games like Crystalis that use
	// single-screen mirroring during cave/indoor transitions.
	Mapper::Mirroring mirroring = Mapper::Mirroring::Horizontal; // default fallback
	if (cartridge_) {
		mirroring = cartridge_->get_mirroring();
	} else {
		// Fallback for tests without cartridge
		mirroring = vertical_mirroring_ ? Mapper::Mirroring::Vertical : Mapper::Mirroring::Horizontal;
	}

	// Map logical nametable index (0-3) to physical VRAM page (0 or 1)
	// NES has only 2KB VRAM → 2 physical nametable pages of 1KB each
	uint16_t physical_page = 0;
	switch (mirroring) {
	case Mapper::Mirroring::Horizontal:
		// NT0,NT1 → page 0; NT2,NT3 → page 1
		physical_page = (nt_index >= 2) ? 1 : 0;
		break;
	case Mapper::Mirroring::Vertical:
		// NT0,NT2 → page 0; NT1,NT3 → page 1
		physical_page = (nt_index & 1);
		break;
	case Mapper::Mirroring::SingleScreenLow:
		// All nametables → page 0
		physical_page = 0;
		break;
	case Mapper::Mirroring::SingleScreenHigh:
		// All nametables → page 1
		physical_page = 1;
		break;
	case Mapper::Mirroring::FourScreen:
		// Four-screen: use full 4KB (not typical NES, needs extra RAM on cart)
		// Clamp to 2KB for safety; true four-screen carts provide their own RAM
		physical_page = nt_index & 1;
		break;
	}

	uint16_t mapped = (physical_page * 0x0400) + nt_offset;

	// Ensure we stay within VRAM bounds
	return mapped % vram_.size();
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
