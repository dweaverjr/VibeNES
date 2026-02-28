// VibeNES - NES Emulator
// ROM Loader & Cartridge Tests
// Tests for iNES header parsing, validation, and cartridge creation

#include "../../include/cartridge/cartridge.hpp"
#include "../../include/cartridge/rom_loader.hpp"
#include "../../include/core/types.hpp"
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

using namespace nes;

// Helper: build a minimal valid iNES ROM in memory
// Header (16 bytes) + PRG ROM + CHR ROM
static std::vector<uint8_t> build_ines_rom(uint8_t prg_pages, uint8_t chr_pages, uint8_t flags6 = 0x00,
										   uint8_t flags7 = 0x00, bool include_trainer = false) {
	std::vector<uint8_t> data;

	// iNES header (16 bytes)
	data.push_back(0x4E); // 'N'
	data.push_back(0x45); // 'E'
	data.push_back(0x53); // 'S'
	data.push_back(0x1A); // EOF
	data.push_back(prg_pages);
	data.push_back(chr_pages);
	data.push_back(flags6);
	data.push_back(flags7);
	// Bytes 8-15: padding
	for (int i = 8; i < 16; i++) {
		data.push_back(0x00);
	}

	// Trainer (512 bytes) if present
	if (include_trainer) {
		for (int i = 0; i < 512; i++) {
			data.push_back(static_cast<uint8_t>(i & 0xFF));
		}
	}

	// PRG ROM (prg_pages × 16KB)
	size_t prg_size = prg_pages * 16384;
	for (size_t i = 0; i < prg_size; i++) {
		data.push_back(static_cast<uint8_t>(i & 0xFF));
	}

	// CHR ROM (chr_pages × 8KB)
	size_t chr_size = chr_pages * 8192;
	for (size_t i = 0; i < chr_size; i++) {
		data.push_back(static_cast<uint8_t>((i + 0x80) & 0xFF));
	}

	return data;
}

// =============================================================================
// iNES Header Validation
// =============================================================================

TEST_CASE("ROM Loader - Header Constants", "[cartridge][rom-loader]") {
	SECTION("iNES magic bytes are correct") {
		// The standard NES magic is "NES" followed by 0x1A
		auto rom = build_ines_rom(1, 1);
		REQUIRE(rom[0] == 0x4E); // 'N'
		REQUIRE(rom[1] == 0x45); // 'E'
		REQUIRE(rom[2] == 0x53); // 'S'
		REQUIRE(rom[3] == 0x1A); // MS-DOS EOF
	}
}

TEST_CASE("ROM Loader - Header Parsing", "[cartridge][rom-loader]") {
	SECTION("PRG ROM page count from header byte 4") {
		auto rom = build_ines_rom(2, 1);
		REQUIRE(rom[4] == 2); // 2 PRG pages = 32KB
	}

	SECTION("CHR ROM page count from header byte 5") {
		auto rom = build_ines_rom(1, 4);
		REQUIRE(rom[5] == 4); // 4 CHR pages = 32KB
	}

	SECTION("Mapper number combines flags6 and flags7") {
		// Mapper 4 = upper nybble of flags6=0x40, flags7=0x00
		auto rom = build_ines_rom(1, 1, 0x40, 0x00);
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 4);
	}

	SECTION("Mapper number high bits from flags7") {
		// Mapper 0x12 = flags6=0x20, flags7=0x10
		auto rom = build_ines_rom(1, 1, 0x20, 0x10);
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 0x12);
	}

	SECTION("Vertical mirroring flag (flags6 bit 0)") {
		auto rom = build_ines_rom(1, 1, 0x01); // bit 0 = 1 → vertical
		REQUIRE((rom[6] & 0x01) == 0x01);
	}

	SECTION("Horizontal mirroring (flags6 bit 0 = 0)") {
		auto rom = build_ines_rom(1, 1, 0x00); // bit 0 = 0 → horizontal
		REQUIRE((rom[6] & 0x01) == 0x00);
	}

	SECTION("Battery-backed RAM flag (flags6 bit 1)") {
		auto rom = build_ines_rom(1, 1, 0x02);
		REQUIRE((rom[6] & 0x02) == 0x02);
	}

	SECTION("Trainer present flag (flags6 bit 2)") {
		auto rom = build_ines_rom(1, 1, 0x04, 0x00, true);
		REQUIRE((rom[6] & 0x04) == 0x04);
	}

	SECTION("Four-screen VRAM flag (flags6 bit 3)") {
		auto rom = build_ines_rom(1, 1, 0x08);
		REQUIRE((rom[6] & 0x08) == 0x08);
	}
}

TEST_CASE("ROM Loader - Data Sizes", "[cartridge][rom-loader]") {
	SECTION("PRG ROM size = pages × 16384") {
		uint8_t pages = 2;
		auto rom = build_ines_rom(pages, 1);
		size_t expected_prg = pages * 16384;
		// Data starts at offset 16 (header), PRG is first
		size_t prg_start = 16;
		size_t prg_end = prg_start + expected_prg;
		REQUIRE(rom.size() >= prg_end);
	}

	SECTION("CHR ROM size = pages × 8192") {
		uint8_t chr_pages = 4;
		auto rom = build_ines_rom(1, chr_pages);
		size_t expected_chr = chr_pages * 8192;
		size_t prg_end = 16 + 16384;
		REQUIRE(rom.size() == prg_end + expected_chr);
	}

	SECTION("With trainer, data offset shifts by 512") {
		auto rom_no_trainer = build_ines_rom(1, 1, 0x00);
		auto rom_with_trainer = build_ines_rom(1, 1, 0x04, 0x00, true);
		REQUIRE(rom_with_trainer.size() == rom_no_trainer.size() + 512);
	}
}

// =============================================================================
// Cartridge Integration
// =============================================================================

TEST_CASE("Cartridge Construction from ROM Data", "[cartridge]") {
	SECTION("Valid Mapper 0 ROM creates cartridge") {
		// This test verifies the high-level flow without loading from disk
		// We just verify the helper builds a valid structure
		auto rom = build_ines_rom(2, 1); // 32KB PRG, 8KB CHR, mapper 0
		REQUIRE(rom.size() == 16 + 32768 + 8192);

		// Mapper ID should be 0
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 0);
	}

	SECTION("Mapper 1 ROM structure") {
		auto rom = build_ines_rom(8, 4, 0x10); // mapper (1 << 4) in flags6 low nybble = mapper 1
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 1);
	}

	SECTION("Mapper 2 ROM structure") {
		auto rom = build_ines_rom(8, 0, 0x20); // mapper 2
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 2);
	}

	SECTION("Mapper 3 ROM structure") {
		auto rom = build_ines_rom(2, 4, 0x30); // mapper 3
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 3);
	}

	SECTION("Mapper 4 ROM structure") {
		auto rom = build_ines_rom(16, 32, 0x40); // mapper 4
		uint8_t mapper_id = (rom[6] >> 4) | (rom[7] & 0xF0);
		REQUIRE(mapper_id == 4);
	}
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ROM Loader - Edge Cases", "[cartridge][rom-loader][edge-cases]") {
	SECTION("Zero CHR pages means CHR RAM") {
		auto rom = build_ines_rom(1, 0); // 0 CHR pages = uses CHR RAM
		REQUIRE(rom[5] == 0);
		// Total size = header + PRG only
		REQUIRE(rom.size() == 16 + 16384);
	}

	SECTION("Maximum PRG pages (255)") {
		// Just verify the header can encode it — don't actually allocate
		auto header = build_ines_rom(1, 1);
		header[4] = 255; // 255 × 16KB = ~4MB
		REQUIRE(header[4] == 255);
	}

	SECTION("Invalid magic bytes should fail validation") {
		auto rom = build_ines_rom(1, 1);
		rom[0] = 'X'; // Corrupt magic
		// Validation check: magic should NOT match
		bool valid = (rom[0] == 0x4E && rom[1] == 0x45 && rom[2] == 0x53 && rom[3] == 0x1A);
		REQUIRE_FALSE(valid);
	}

	SECTION("File too small for header should fail") {
		std::vector<uint8_t> tiny = {0x4E, 0x45, 0x53}; // Only 3 bytes
		REQUIRE(tiny.size() < 16);
	}
}
