#pragma once

#include "cartridge/cartridge.hpp"
#include "cartridge/rom_loader.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

namespace nes::test {

/// Test CHR ROM data for PPU testing
/// Creates synthetic pattern table data with known patterns for sprite 0 hit testing
class TestCHRData {
  public:
	/// Create synthetic ROM data for testing
	static RomData create_test_rom_data() {
		RomData rom_data;

		// Header information (NROM mapper)
		rom_data.mapper_id = 0;				 // NROM
		rom_data.prg_rom_pages = 2;			 // 32KB PRG ROM
		rom_data.chr_rom_pages = 1;			 // 8KB CHR ROM
		rom_data.vertical_mirroring = false; // Horizontal mirroring
		rom_data.battery_backed_ram = false;
		rom_data.trainer_present = false;
		rom_data.four_screen_vram = false;

		// Create minimal PRG ROM (32KB)
		rom_data.prg_rom.resize(32768, 0xEA); // Fill with NOP instructions

		// Create test CHR ROM data
		rom_data.chr_rom = create_test_chr_data();

		// No trainer
		rom_data.trainer.clear();

		// File info
		rom_data.filename = "test_rom.nes";
		rom_data.valid = true;

		return rom_data;
	}

	/// Create a test cartridge with synthetic CHR ROM data
	static std::shared_ptr<Cartridge> create_test_cartridge() {
		auto cartridge = std::make_shared<Cartridge>();

		// Create synthetic ROM data
		RomData rom_data = create_test_rom_data();

		// Load the synthetic ROM data
		if (!cartridge->load_from_rom_data(rom_data)) {
			std::cerr << "Failed to load test ROM data into cartridge!" << std::endl;
			return nullptr;
		}

		return cartridge;
	}

	/// Create 8KB of test CHR ROM data with known patterns
	static std::vector<uint8_t> create_test_chr_data() {
		std::vector<uint8_t> chr_data(8192, 0x00); // 8KB CHR ROM

		// Pattern Table 0: $0000-$0FFF (background tiles)
		// Pattern Table 1: $1000-$1FFF (sprite tiles)

		// Create tile 0x01 with a solid pattern (for sprite 0 hit testing)
		create_solid_tile(chr_data, 0x01, 0xFF); // Tile 1 = solid pixels

		// Create tile 0x00 with transparent pattern
		create_solid_tile(chr_data, 0x00, 0x00); // Tile 0 = transparent

		// Create some background tiles in pattern table 0
		for (int tile = 0; tile < 16; ++tile) {
			create_test_pattern_tile(chr_data, tile, tile);
		}

		// Create sprite tiles in pattern table 1 (offset by 0x1000)
		for (int tile = 0; tile < 16; ++tile) {
			create_test_pattern_tile(chr_data, 0x100 + tile, tile);
		}

		return chr_data;
	}

  private:
	/// Create a solid tile pattern (8x8 pixels)
	static void create_solid_tile(std::vector<uint8_t> &chr_data, uint8_t tile_index, uint8_t pattern) {
		uint16_t tile_offset = tile_index * 16; // Each tile is 16 bytes

		// Low bit plane (8 bytes)
		for (int row = 0; row < 8; ++row) {
			chr_data[tile_offset + row] = pattern;
		}

		// High bit plane (8 bytes)
		for (int row = 0; row < 8; ++row) {
			chr_data[tile_offset + 8 + row] = pattern;
		}
	}

	/// Create a test pattern tile (checkerboard, stripes, etc.)
	static void create_test_pattern_tile(std::vector<uint8_t> &chr_data, uint16_t tile_index, uint8_t pattern_type) {
		uint16_t tile_offset = tile_index * 16;

		switch (pattern_type % 4) {
		case 0: // Solid color
			create_solid_tile(chr_data, tile_index, 0xFF);
			break;
		case 1: // Horizontal stripes
			for (int row = 0; row < 8; ++row) {
				uint8_t stripe = (row % 2) ? 0xFF : 0x00;
				chr_data[tile_offset + row] = stripe;
				chr_data[tile_offset + 8 + row] = stripe;
			}
			break;
		case 2: // Vertical stripes
			for (int row = 0; row < 8; ++row) {
				chr_data[tile_offset + row] = 0xAA;		// 10101010
				chr_data[tile_offset + 8 + row] = 0x55; // 01010101
			}
			break;
		case 3: // Checkerboard
			for (int row = 0; row < 8; ++row) {
				uint8_t checker = (row % 2) ? 0xAA : 0x55;
				chr_data[tile_offset + row] = checker;
				chr_data[tile_offset + 8 + row] = ~checker;
			}
			break;
		}
	}
};

} // namespace nes::test
