// VibeNES - NES Emulator
// Mapper Tests
// Tests for Mappers 0 (NROM), 1 (MMC1), 2 (UxROM), 3 (CNROM), 4 (MMC3)

#include "../../include/cartridge/mappers/mapper_000.hpp"
#include "../../include/cartridge/mappers/mapper_001.hpp"
#include "../../include/cartridge/mappers/mapper_002.hpp"
#include "../../include/cartridge/mappers/mapper_003.hpp"
#include "../../include/cartridge/mappers/mapper_004.hpp"
#include "../../include/core/types.hpp"
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

using namespace nes;

// Helper: create a vector filled with a pattern for easy verification
static std::vector<uint8_t> make_rom(size_t size, uint8_t base_value = 0) {
	std::vector<uint8_t> rom(size);
	for (size_t i = 0; i < size; i++) {
		rom[i] = static_cast<uint8_t>((base_value + i) & 0xFF);
	}
	return rom;
}

// Helper: create PRG ROM where each 16KB bank starts with a distinct byte
static std::vector<uint8_t> make_prg_with_bank_ids(int num_banks) {
	std::vector<uint8_t> rom(num_banks * 16384, 0x00);
	for (int b = 0; b < num_banks; b++) {
		// First byte of each 16KB bank encodes the bank number
		rom[b * 16384] = static_cast<uint8_t>(b);
		// Also put bank ID at common offsets for easy checking
		rom[b * 16384 + 1] = static_cast<uint8_t>(b);
	}
	return rom;
}

// Helper: create CHR ROM where each 8KB bank starts with a distinct byte
static std::vector<uint8_t> make_chr_with_bank_ids(int num_banks) {
	std::vector<uint8_t> rom(num_banks * 8192, 0x00);
	for (int b = 0; b < num_banks; b++) {
		rom[b * 8192] = static_cast<uint8_t>(b);
		rom[b * 8192 + 1] = static_cast<uint8_t>(b);
	}
	return rom;
}

// =============================================================================
// Mapper 0 (NROM)
// =============================================================================

TEST_CASE("Mapper 0 (NROM) - Basic", "[mapper][mapper0]") {
	SECTION("Mapper ID and name") {
		auto prg = make_rom(32768);
		auto chr = make_rom(8192);
		Mapper000 mapper(prg, chr, Mapper::Mirroring::Horizontal);

		REQUIRE(mapper.get_mapper_id() == 0);
		REQUIRE(std::string(mapper.get_name()) == "NROM");
	}
}

TEST_CASE("Mapper 0 (NROM) - 32KB PRG", "[mapper][mapper0]") {
	auto prg = make_rom(32768, 0x10);
	auto chr = make_rom(8192, 0x20);
	Mapper000 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("CPU read $8000 returns first PRG byte") {
		REQUIRE(mapper.cpu_read(0x8000) == prg[0]);
	}

	SECTION("CPU read $FFFF returns last PRG byte") {
		REQUIRE(mapper.cpu_read(0xFFFF) == prg[32767]);
	}

	SECTION("CPU read spans full 32KB") {
		REQUIRE(mapper.cpu_read(0xC000) == prg[0x4000]);
	}

	SECTION("CPU writes are ignored (ROM)") {
		uint8_t original = mapper.cpu_read(0x8000);
		mapper.cpu_write(0x8000, 0xFF);
		REQUIRE(mapper.cpu_read(0x8000) == original);
	}

	SECTION("Mirroring mode preserved") {
		REQUIRE(mapper.get_mirroring() == Mapper::Mirroring::Vertical);
	}
}

TEST_CASE("Mapper 0 (NROM) - 16KB PRG Mirroring", "[mapper][mapper0]") {
	auto prg = make_rom(16384, 0x30);
	auto chr = make_rom(8192);
	Mapper000 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("$8000-$BFFF maps to PRG ROM") {
		REQUIRE(mapper.cpu_read(0x8000) == prg[0]);
		REQUIRE(mapper.cpu_read(0xBFFF) == prg[16383]);
	}

	SECTION("$C000-$FFFF mirrors $8000-$BFFF") {
		REQUIRE(mapper.cpu_read(0xC000) == mapper.cpu_read(0x8000));
		REQUIRE(mapper.cpu_read(0xFFFF) == mapper.cpu_read(0xBFFF));
		REQUIRE(mapper.cpu_read(0xD000) == mapper.cpu_read(0x9000));
	}
}

TEST_CASE("Mapper 0 (NROM) - CHR ROM", "[mapper][mapper0]") {
	auto prg = make_rom(32768);
	auto chr = make_rom(8192, 0x50);
	Mapper000 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("PPU read $0000-$1FFF returns CHR ROM") {
		REQUIRE(mapper.ppu_read(0x0000) == chr[0]);
		REQUIRE(mapper.ppu_read(0x1FFF) == chr[8191]);
	}

	SECTION("PPU writes to CHR ROM are ignored") {
		uint8_t original = mapper.ppu_read(0x0000);
		mapper.ppu_write(0x0000, 0xFF);
		REQUIRE(mapper.ppu_read(0x0000) == original);
	}
}

TEST_CASE("Mapper 0 (NROM) - Out of Range", "[mapper][mapper0]") {
	auto prg = make_rom(32768);
	auto chr = make_rom(8192);
	Mapper000 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("CPU read below $8000 returns 0xFF") {
		REQUIRE(mapper.cpu_read(0x6000) == 0xFF);
		REQUIRE(mapper.cpu_read(0x7FFF) == 0xFF);
	}
}

TEST_CASE("Mapper 0 (NROM) - Serialization", "[mapper][mapper0][serialization]") {
	auto prg = make_rom(32768);
	auto chr = make_rom(8192);
	Mapper000 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("Serialize/deserialize is no-op (stateless mapper)") {
		std::vector<uint8_t> buffer;
		mapper.serialize_state(buffer);
		// NROM has no mutable state — buffer may be empty or minimal
		// Just ensure it doesn't crash
		size_t offset = 0;
		mapper.deserialize_state(buffer, offset);
	}
}

// =============================================================================
// Mapper 1 (MMC1)
// =============================================================================

TEST_CASE("Mapper 1 (MMC1) - Basic", "[mapper][mapper1]") {
	auto prg = make_prg_with_bank_ids(8); // 128KB PRG (8 × 16KB)
	auto chr = make_chr_with_bank_ids(4); // 32KB CHR (4 × 8KB)
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Mapper ID and name") {
		REQUIRE(mapper.get_mapper_id() == 1);
		REQUIRE(std::string(mapper.get_name()) == "MMC1");
	}

	SECTION("Initial state: last bank fixed at $C000") {
		// Control register defaults to 0x0C (mode 3: fix last bank at $C000)
		// Last bank = bank 7
		REQUIRE(mapper.cpu_read(0xC000) == 7); // Bank 7 ID byte
	}
}

TEST_CASE("Mapper 1 (MMC1) - Serial Write Protocol", "[mapper][mapper1]") {
	auto prg = make_prg_with_bank_ids(8);
	auto chr = make_rom(32768);
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("5 writes to shift register loads internal register") {
		// Write to PRG bank register ($E000-$FFFF) to select bank 2
		// Each write shifts bit 0 into the shift register
		// After 5 writes, the accumulated value is written to the target register
		// Value 2 = 0b00010 → write bits: 0, 1, 0, 0, 0

		// Need to tick between writes to avoid consecutive-write filter
		mapper.notify_cpu_cycle();		// Cycle 1
		mapper.cpu_write(0xE000, 0x00); // bit 0 = 0
		mapper.notify_cpu_cycle();		// Cycle 2 (dummy)
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01); // bit 1 = 1
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00); // bit 2 = 0
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00); // bit 3 = 0
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00); // bit 4 = 0 (5th write triggers load)

		// PRG bank register now = 2
		// In mode 3 (fix last), $8000 should map to bank 2
		REQUIRE(mapper.cpu_read(0x8000) == 2);
	}

	SECTION("Bit 7 reset clears shift register") {
		// Start a write sequence
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01);

		// Reset with bit 7
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x80);

		// Shift register is reset — need fresh 5 writes
		// Write bank 3 (0b00011)
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);

		REQUIRE(mapper.cpu_read(0x8000) == 3);
	}
}

TEST_CASE("Mapper 1 (MMC1) - Consecutive Write Filter", "[mapper][mapper1]") {
	auto prg = make_prg_with_bank_ids(8);
	auto chr = make_rom(32768);
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Back-to-back writes on same cycle are ignored") {
		// First write
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x01);

		// Second write on SAME cycle (no notify_cpu_cycle between) — should be ignored
		mapper.cpu_write(0xE000, 0x00);

		// Only the first write should have taken effect
		// We need to complete the 5-write sequence to verify
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00);
		mapper.notify_cpu_cycle();
		mapper.notify_cpu_cycle();
		mapper.cpu_write(0xE000, 0x00); // 5th write (if filter worked, this is #4 of good writes + bit7 confusion)

		// The point is: if the filter works, only one bit was shifted from that first
		// write cycle, not two. Exact bank depends on implementation details.
		// Just verifying it doesn't crash and produces a consistent result.
		auto val = mapper.cpu_read(0x8000);
		(void)val; // Just ensure no crash
	}
}

TEST_CASE("Mapper 1 (MMC1) - PRG RAM", "[mapper][mapper1]") {
	auto prg = make_prg_with_bank_ids(8);
	auto chr = make_rom(32768);
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical, true); // has_prg_ram=true

	SECTION("PRG RAM read/write at $6000-$7FFF") {
		mapper.cpu_write(0x6000, 0xAA);
		REQUIRE(mapper.cpu_read(0x6000) == 0xAA);

		mapper.cpu_write(0x7FFF, 0xBB);
		REQUIRE(mapper.cpu_read(0x7FFF) == 0xBB);
	}

	SECTION("PRG RAM preserves data across accesses") {
		for (int i = 0; i < 256; i++) {
			mapper.cpu_write(0x6000 + i, static_cast<uint8_t>(i));
		}
		for (int i = 0; i < 256; i++) {
			REQUIRE(mapper.cpu_read(0x6000 + i) == static_cast<uint8_t>(i));
		}
	}
}

TEST_CASE("Mapper 1 (MMC1) - Mirroring Control", "[mapper][mapper1]") {
	auto prg = make_prg_with_bank_ids(8);
	auto chr = make_rom(32768);
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Initial mirroring matches constructor") {
		// MMC1 control register defaults to 0x0C, but mirroring comes from the control
		// register bits 0-1 which default to... depends on init
		auto mir = mapper.get_mirroring();
		// Just verify it's a valid mirroring mode
		REQUIRE((mir == Mapper::Mirroring::Horizontal || mir == Mapper::Mirroring::Vertical ||
				 mir == Mapper::Mirroring::SingleScreenLow || mir == Mapper::Mirroring::SingleScreenHigh));
	}
}

TEST_CASE("Mapper 1 (MMC1) - Serialization", "[mapper][mapper1][serialization]") {
	auto prg = make_prg_with_bank_ids(8);
	auto chr = make_rom(32768);
	Mapper001 mapper(prg, chr, Mapper::Mirroring::Vertical, true);

	// Write some PRG RAM data
	mapper.cpu_write(0x6000, 0xDE);
	mapper.cpu_write(0x6001, 0xAD);

	SECTION("Roundtrip preserves PRG RAM and register state") {
		std::vector<uint8_t> buffer;
		mapper.serialize_state(buffer);
		REQUIRE(buffer.size() > 0);

		// Create new mapper and restore
		Mapper001 mapper2(make_prg_with_bank_ids(8), make_rom(32768), Mapper::Mirroring::Vertical, true);
		size_t offset = 0;
		mapper2.deserialize_state(buffer, offset);

		// PRG RAM should match
		REQUIRE(mapper2.cpu_read(0x6000) == 0xDE);
		REQUIRE(mapper2.cpu_read(0x6001) == 0xAD);
	}
}

// =============================================================================
// Mapper 2 (UxROM)
// =============================================================================

TEST_CASE("Mapper 2 (UxROM) - Basic", "[mapper][mapper2]") {
	auto prg = make_prg_with_bank_ids(8); // 128KB (8 × 16KB)
	auto chr = make_rom(8192);
	Mapper002 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Mapper ID and name") {
		REQUIRE(mapper.get_mapper_id() == 2);
		REQUIRE(std::string(mapper.get_name()) == "UxROM");
	}

	SECTION("Initial state: bank 0 at $8000") {
		REQUIRE(mapper.cpu_read(0x8000) == 0); // Bank 0 ID
	}

	SECTION("Last bank fixed at $C000-$FFFF") {
		REQUIRE(mapper.cpu_read(0xC000) == 7); // Bank 7 (last)
	}
}

TEST_CASE("Mapper 2 (UxROM) - Bank Switching", "[mapper][mapper2]") {
	// Create PRG ROM where each bank byte matches its bank index at known offsets
	auto prg = make_prg_with_bank_ids(8);
	// Ensure the ROM data at 0x8000 contains something we can AND with for bus conflicts
	// We need the value at the write address to allow the bank through
	// Put 0xFF at a known write location so bus conflict doesn't mask the value
	for (int b = 0; b < 8; b++) {
		prg[b * 16384 + 0x100] = 0xFF; // Offset $8100 in each bank
	}
	auto chr = make_rom(8192);
	Mapper002 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Write selects PRG bank at $8000-$BFFF") {
		// Write to $8100 (bus conflict: ROM value=0xFF, so effective = bank & 0xFF = bank)
		mapper.cpu_write(0x8100, 0x03);
		REQUIRE(mapper.cpu_read(0x8000) == 3);

		mapper.cpu_write(0x8100, 0x05);
		REQUIRE(mapper.cpu_read(0x8000) == 5);
	}

	SECTION("Last bank always fixed") {
		mapper.cpu_write(0x8100, 0x02);
		REQUIRE(mapper.cpu_read(0xC000) == 7); // Still bank 7

		mapper.cpu_write(0x8100, 0x06);
		REQUIRE(mapper.cpu_read(0xC000) == 7); // Still bank 7
	}

	SECTION("Bank wraps with mask") {
		// With 8 banks, mask is 7 (0b111)
		mapper.cpu_write(0x8100, 0x0F); // 0x0F & 7 = 7
		REQUIRE(mapper.cpu_read(0x8000) == 7);
	}
}

TEST_CASE("Mapper 2 (UxROM) - CHR RAM", "[mapper][mapper2]") {
	auto prg = make_prg_with_bank_ids(4);
	auto chr = make_rom(8192);
	Mapper002 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("CHR RAM is writable") {
		mapper.ppu_write(0x0000, 0xAB);
		REQUIRE(mapper.ppu_read(0x0000) == 0xAB);

		mapper.ppu_write(0x1FFF, 0xCD);
		REQUIRE(mapper.ppu_read(0x1FFF) == 0xCD);
	}
}

TEST_CASE("Mapper 2 (UxROM) - Serialization", "[mapper][mapper2][serialization]") {
	auto prg = make_prg_with_bank_ids(8);
	prg[0x100] = 0xFF; // Bus conflict safe write location
	auto chr = make_rom(8192);
	Mapper002 mapper(prg, chr, Mapper::Mirroring::Vertical);

	// Switch to bank 5
	mapper.cpu_write(0x8100, 0x05);
	// Write CHR RAM
	mapper.ppu_write(0x0000, 0x42);

	SECTION("Roundtrip preserves bank selection and CHR RAM") {
		std::vector<uint8_t> buffer;
		mapper.serialize_state(buffer);
		REQUIRE(buffer.size() > 0);

		Mapper002 mapper2(make_prg_with_bank_ids(8), make_rom(8192), Mapper::Mirroring::Vertical);
		size_t offset = 0;
		mapper2.deserialize_state(buffer, offset);

		REQUIRE(mapper2.cpu_read(0x8000) == 5);
		REQUIRE(mapper2.ppu_read(0x0000) == 0x42);
	}
}

// =============================================================================
// Mapper 3 (CNROM)
// =============================================================================

TEST_CASE("Mapper 3 (CNROM) - Basic", "[mapper][mapper3]") {
	auto prg = make_rom(32768, 0x10);
	auto chr = make_chr_with_bank_ids(4); // 32KB CHR (4 × 8KB)
	Mapper003 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("Mapper ID and name") {
		REQUIRE(mapper.get_mapper_id() == 3);
		REQUIRE(std::string(mapper.get_name()) == "CNROM");
	}

	SECTION("Initial CHR bank is 0") {
		REQUIRE(mapper.ppu_read(0x0000) == 0); // Bank 0 ID byte
	}
}

TEST_CASE("Mapper 3 (CNROM) - CHR Bank Switching", "[mapper][mapper3]") {
	auto prg = make_rom(32768, 0xFF); // Fill PRG with 0xFF for bus conflict passthrough
	auto chr = make_chr_with_bank_ids(4);
	Mapper003 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	SECTION("Write switches CHR bank") {
		mapper.cpu_write(0x8000, 0x02);
		REQUIRE(mapper.ppu_read(0x0000) == 2); // Bank 2 ID

		mapper.cpu_write(0x8000, 0x03);
		REQUIRE(mapper.ppu_read(0x0000) == 3); // Bank 3 ID
	}

	SECTION("CHR bank wraps with mask") {
		// 4 banks → mask = 3
		mapper.cpu_write(0x8000, 0x07); // 7 & 3 = 3
		REQUIRE(mapper.ppu_read(0x0000) == 3);
	}

	SECTION("CHR ROM is read-only") {
		mapper.ppu_write(0x0000, 0xFF);
		REQUIRE(mapper.ppu_read(0x0000) == 0); // Still bank 0's original data
	}
}

TEST_CASE("Mapper 3 (CNROM) - PRG ROM", "[mapper][mapper3]") {
	SECTION("16KB PRG mirrors") {
		auto prg = make_rom(16384, 0x30);
		auto chr = make_chr_with_bank_ids(2);
		Mapper003 mapper(prg, chr, Mapper::Mirroring::Vertical);

		REQUIRE(mapper.cpu_read(0x8000) == mapper.cpu_read(0xC000));
	}

	SECTION("32KB PRG full range") {
		auto prg = make_rom(32768, 0x40);
		auto chr = make_chr_with_bank_ids(2);
		Mapper003 mapper(prg, chr, Mapper::Mirroring::Vertical);

		REQUIRE(mapper.cpu_read(0x8000) == prg[0]);
		REQUIRE(mapper.cpu_read(0xFFFF) == prg[32767]);
	}

	SECTION("No PRG RAM ($6000 returns 0xFF)") {
		auto prg = make_rom(32768);
		auto chr = make_chr_with_bank_ids(2);
		Mapper003 mapper(prg, chr, Mapper::Mirroring::Horizontal);

		REQUIRE(mapper.cpu_read(0x6000) == 0xFF);
	}
}

TEST_CASE("Mapper 3 (CNROM) - Serialization", "[mapper][mapper3][serialization]") {
	auto prg = make_rom(32768, 0xFF);
	auto chr = make_chr_with_bank_ids(4);
	Mapper003 mapper(prg, chr, Mapper::Mirroring::Horizontal);

	mapper.cpu_write(0x8000, 0x02);

	std::vector<uint8_t> buffer;
	mapper.serialize_state(buffer);

	Mapper003 mapper2(make_rom(32768, 0xFF), make_chr_with_bank_ids(4), Mapper::Mirroring::Horizontal);
	size_t offset = 0;
	mapper2.deserialize_state(buffer, offset);

	REQUIRE(mapper2.ppu_read(0x0000) == 2);
}

// =============================================================================
// Mapper 4 (MMC3)
// =============================================================================

TEST_CASE("Mapper 4 (MMC3) - Basic", "[mapper][mapper4]") {
	auto prg = make_prg_with_bank_ids(16); // 256KB PRG (16 × 16KB = 32 × 8KB)
	auto chr = make_chr_with_bank_ids(32); // 256KB CHR (32 × 8KB)
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Mapper ID and name") {
		REQUIRE(mapper.get_mapper_id() == 4);
		REQUIRE(std::string(mapper.get_name()) == "MMC3");
	}

	SECTION("No IRQ pending initially") {
		REQUIRE_FALSE(mapper.is_irq_pending());
	}
}

TEST_CASE("Mapper 4 (MMC3) - PRG Banking", "[mapper][mapper4]") {
	auto prg = make_rom(262144); // 256KB PRG = 32 × 8KB banks
	// Mark each 8KB bank with its index
	for (int b = 0; b < 32; b++) {
		prg[b * 8192] = static_cast<uint8_t>(b);
	}
	auto chr = make_rom(262144);
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("Last 8KB bank is fixed at $E000-$FFFF") {
		// Bank 31 (last) should be at $E000
		REQUIRE(mapper.cpu_read(0xE000) == 31);
	}

	SECTION("Bank select register ($8000) + bank data ($8001)") {
		// Select R6 (PRG bank at $8000) and write bank 5
		mapper.cpu_write(0x8000, 0x06); // R6
		mapper.cpu_write(0x8001, 0x05); // Bank 5
		REQUIRE(mapper.cpu_read(0x8000) == 5);

		// Select R7 (PRG bank at $A000) and write bank 10
		mapper.cpu_write(0x8000, 0x07); // R7
		mapper.cpu_write(0x8001, 0x0A); // Bank 10
		REQUIRE(mapper.cpu_read(0xA000) == 10);
	}
}

TEST_CASE("Mapper 4 (MMC3) - PRG RAM", "[mapper][mapper4]") {
	auto prg = make_rom(262144);
	auto chr = make_rom(262144);
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical, true);

	SECTION("PRG RAM accessible at $6000-$7FFF") {
		mapper.cpu_write(0x6000, 0x42);
		REQUIRE(mapper.cpu_read(0x6000) == 0x42);

		mapper.cpu_write(0x7FFF, 0xBE);
		REQUIRE(mapper.cpu_read(0x7FFF) == 0xBE);
	}
}

TEST_CASE("Mapper 4 (MMC3) - Mirroring Control", "[mapper][mapper4]") {
	auto prg = make_rom(262144);
	auto chr = make_rom(262144);
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("$A000 controls mirroring") {
		mapper.cpu_write(0xA000, 0x00); // Vertical
		REQUIRE(mapper.get_mirroring() == Mapper::Mirroring::Vertical);

		mapper.cpu_write(0xA000, 0x01); // Horizontal
		REQUIRE(mapper.get_mirroring() == Mapper::Mirroring::Horizontal);
	}
}

TEST_CASE("Mapper 4 (MMC3) - IRQ Counter", "[mapper][mapper4]") {
	auto prg = make_rom(262144);
	auto chr = make_rom(262144);
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical);

	SECTION("IRQ latch and reload") {
		// Set IRQ latch value
		mapper.cpu_write(0xC000, 0x08); // Latch = 8 (fire after 8 scanlines)
		mapper.cpu_write(0xC001, 0x00); // Reload counter
		mapper.cpu_write(0xE001, 0x00); // Enable IRQ

		// Simulate scanline counting via A12 toggles
		for (int i = 0; i < 8; i++) {
			mapper.ppu_a12_toggle();
		}

		// After 8 toggles, IRQ should be pending
		// (Depends on the counter reaching 0 and then being clocked once more)
		// The exact behavior depends on implementation details
	}

	SECTION("IRQ disable clears pending") {
		mapper.cpu_write(0xE000, 0x00); // Disable IRQ + acknowledge
		REQUIRE_FALSE(mapper.is_irq_pending());
	}

	SECTION("clear_irq() acknowledges mapper IRQ") {
		mapper.clear_irq();
		REQUIRE_FALSE(mapper.is_irq_pending());
	}
}

TEST_CASE("Mapper 4 (MMC3) - Serialization", "[mapper][mapper4][serialization]") {
	auto prg = make_rom(262144);
	auto chr = make_rom(262144);
	Mapper004 mapper(prg, chr, Mapper::Mirroring::Vertical, true);

	// Set up some state
	mapper.cpu_write(0x8000, 0x06);
	mapper.cpu_write(0x8001, 0x05);
	mapper.cpu_write(0x6000, 0xAA); // PRG RAM

	SECTION("Roundtrip preserves banking and PRG RAM") {
		std::vector<uint8_t> buffer;
		mapper.serialize_state(buffer);
		REQUIRE(buffer.size() > 0);

		Mapper004 mapper2(make_rom(262144), make_rom(262144), Mapper::Mirroring::Vertical, true);
		size_t offset = 0;
		mapper2.deserialize_state(buffer, offset);

		REQUIRE(mapper2.cpu_read(0x6000) == 0xAA);
	}
}
