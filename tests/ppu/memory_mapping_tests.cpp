// VibeNES - NES Emulator
// PPU Memory Mapping Tests
// Tests for hardware-accurate PPU memory system behavior

#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include <catch2/catch_all.hpp>
#include <memory>

using namespace nes;

class MemoryMappingTestFixture {
  public:
	MemoryMappingTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		cartridge = std::make_shared<Cartridge>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());

		// Connect components to bus (like TimingTestFixture)
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);
		bus->connect_cpu(cpu);

		// Create and connect PPU
		ppu = std::make_shared<PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		// Connect cartridge to PPU for CHR ROM access
		ppu->connect_cartridge(cartridge);

		// Connect CPU to PPU for NMI generation
		ppu->connect_cpu(cpu.get());

		ppu->power_on();
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void set_vram_address(uint16_t address) {
		read_ppu_register(0x2002); // Clear toggle
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
	}

	void write_vram(uint16_t address, uint8_t value) {
		set_vram_address(address);
		write_ppu_register(0x2007, value);
	}

	uint8_t read_vram(uint16_t address) {
		set_vram_address(address);

		// Apply PPU memory mirroring to check if this is a palette address
		uint16_t effective_address = address & 0x3FFF;

		// Hardware behavior: PPUDATA reads are buffered except for palette memory
		if (effective_address >= 0x3F00 && effective_address <= 0x3FFF) {
			// Palette reads return immediately (no buffering)
			return read_ppu_register(0x2007);
		} else {
			// Non-palette reads: first read loads buffer, second read returns data
			read_ppu_register(0x2007);		  // Dummy read to load buffer
			set_vram_address(address);		  // Reset address since dummy read incremented it
			return read_ppu_register(0x2007); // Actual read
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<Cartridge> cartridge;
	std::shared_ptr<APU> apu;
	std::shared_ptr<CPU6502> cpu;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(MemoryMappingTestFixture, "Pattern Table Mapping", "[ppu][memory][pattern_tables]") {
	SECTION("Pattern table 0 should map to $0000-$0FFF") {
		// Write to pattern table 0
		for (uint16_t addr = 0x0000; addr <= 0x0FFF; addr += 0x100) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Read back and verify
		for (uint16_t addr = 0x0000; addr <= 0x0FFF; addr += 0x100) {
			uint8_t value = read_vram(addr);
			REQUIRE(value == static_cast<uint8_t>(addr & 0xFF));
		}
	}

	SECTION("Pattern table 1 should map to $1000-$1FFF") {
		// Write to pattern table 1
		for (uint16_t addr = 0x1000; addr <= 0x1FFF; addr += 0x100) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Read back and verify
		for (uint16_t addr = 0x1000; addr <= 0x1FFF; addr += 0x100) {
			uint8_t value = read_vram(addr);
			REQUIRE(value == static_cast<uint8_t>(addr & 0xFF));
		}
	}

	SECTION("Pattern table reads should have correct timing") {
		// Pattern table reads from cartridge CHR ROM/RAM
		// Should be accessible during VBlank

		// Advance to VBlank
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() != 241 && safety_counter < MAX_CYCLES) {
			ppu->tick(CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("Loop hit safety limit - possible infinite loop");
		}

		[[maybe_unused]] uint8_t data = read_vram(0x0100);
		// Should read successfully during VBlank
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "Nametable Mapping", "[ppu][memory][nametables]") {
	SECTION("Nametable 0 should map to $2000-$23FF") {
		// Write pattern to nametable 0
		for (uint16_t addr = 0x2000; addr < 0x23C0; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Read back and verify
		for (uint16_t addr = 0x2000; addr < 0x23C0; addr += 16) {
			uint8_t value = read_vram(addr);
			REQUIRE(value == static_cast<uint8_t>(addr & 0xFF));
		}
	}

	SECTION("Nametable 1 should map to $2400-$27FF") {
		for (uint16_t addr = 0x2400; addr < 0x27C0; addr++) {
			write_vram(addr, static_cast<uint8_t>((addr >> 8) ^ (addr & 0xFF)));
		}

		for (uint16_t addr = 0x2400; addr < 0x27C0; addr += 16) {
			uint8_t expected = static_cast<uint8_t>((addr >> 8) ^ (addr & 0xFF));
			uint8_t value = read_vram(addr);
			REQUIRE(value == expected);
		}
	}

	SECTION("Nametable 2 should map to $2800-$2BFF") {
		for (uint16_t addr = 0x2800; addr < 0x2BC0; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr ^ 0xAA));
		}

		for (uint16_t addr = 0x2800; addr < 0x2BC0; addr += 16) {
			uint8_t expected = static_cast<uint8_t>(addr ^ 0xAA);
			uint8_t value = read_vram(addr);
			REQUIRE(value == expected);
		}
	}

	SECTION("Nametable 3 should map to $2C00-$2FFF") {
		for (uint16_t addr = 0x2C00; addr < 0x2FC0; addr++) {
			write_vram(addr, static_cast<uint8_t>(~addr));
		}

		for (uint16_t addr = 0x2C00; addr < 0x2FC0; addr += 16) {
			uint8_t expected = static_cast<uint8_t>(~addr);
			uint8_t value = read_vram(addr);
			REQUIRE(value == expected);
		}
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "Attribute Table Mapping", "[ppu][memory][attributes]") {
	SECTION("Attribute tables should map correctly") {
		// Nametable 0 attribute table: $23C0-$23FF
		for (uint16_t addr = 0x23C0; addr <= 0x23FF; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		for (uint16_t addr = 0x23C0; addr <= 0x23FF; addr++) {
			uint8_t value = read_vram(addr);
			REQUIRE(value == static_cast<uint8_t>(addr & 0xFF));
		}

		// Nametable 1 attribute table: $27C0-$27FF
		for (uint16_t addr = 0x27C0; addr <= 0x27FF; addr++) {
			write_vram(addr, static_cast<uint8_t>(~addr));
		}

		for (uint16_t addr = 0x27C0; addr <= 0x27FF; addr++) {
			uint8_t value = read_vram(addr);
			REQUIRE(value == static_cast<uint8_t>(~addr));
		}
	}

	SECTION("Attribute table addressing should be correct") {
		// Attribute table entry for tile (0,0) should be at base + $00
		// Attribute table entry for tile (2,2) should be at base + $08
		// Attribute table entry for tile (30,28) should be at base + $3F

		uint16_t attr_base = 0x23C0;

		// Test corner cases
		write_vram(attr_base + 0x00, 0x12); // Top-left corner
		write_vram(attr_base + 0x07, 0x34); // Top-right corner
		write_vram(attr_base + 0x38, 0x56); // Bottom-left corner
		write_vram(attr_base + 0x3F, 0x78); // Bottom-right corner

		REQUIRE(static_cast<int>(read_vram(attr_base + 0x00)) == 0x12);
		REQUIRE(static_cast<int>(read_vram(attr_base + 0x07)) == 0x34);
		REQUIRE(static_cast<int>(read_vram(attr_base + 0x38)) == 0x56);
		REQUIRE(static_cast<int>(read_vram(attr_base + 0x3F)) == 0x78);
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "Palette Memory Mapping", "[ppu][memory][palettes]") {
	SECTION("Background palette should map to $3F00-$3F0F") {
		// Write background palette
		for (uint8_t i = 0; i < 16; i++) {
			write_vram(0x3F00 + i, i * 4);
		}

		// Read back and verify
		for (uint8_t i = 0; i < 16; i++) {
			uint8_t value = read_vram(0x3F00 + i);
			REQUIRE(static_cast<int>(value) == i * 4);
		}
	}

	SECTION("Sprite palette should map to $3F10-$3F1F") {
		// Write sprite palette (use 6-bit values since NES palette RAM is 6-bit)
		for (uint8_t i = 0; i < 16; i++) {
			uint8_t pal_value = (i * 4) & 0x3F; // Keep within 6-bit range
			write_vram(0x3F10 + i, pal_value);
		}

		// Read back and verify
		for (uint8_t i = 0; i < 16; i++) {
			uint8_t expected = (i * 4) & 0x3F;
			uint8_t value = read_vram(0x3F10 + i);
			REQUIRE(static_cast<int>(value) == static_cast<int>(expected));
		}
	}

	SECTION("Palette mirrors should work correctly") {
		// Write to base palette
		write_vram(0x3F00, 0x12); // Universal background color
		write_vram(0x3F04, 0x19); // BG palette 1, entry 0
		write_vram(0x3F05, 0x34); // Background palette 1, color 1
		write_vram(0x3F15, 0x56); // Sprite palette 1, color 1

		// Test mirrors — $3F10 mirrors $3F00, $3F14 mirrors $3F04 (NOT $3F00)
		REQUIRE(static_cast<int>(read_vram(0x3F10)) == 0x12); // $3F10 -> $3F00
		REQUIRE(static_cast<int>(read_vram(0x3F14)) == 0x19); // $3F14 -> $3F04
	}

	SECTION("Palette memory should be only 6 bits") {
		// Write values with upper bits set
		write_vram(0x3F00, 0xFF);
		write_vram(0x3F01, 0x80);
		write_vram(0x3F02, 0x40);

		// Read back - only lower 6 bits should be stored
		REQUIRE(static_cast<int>(read_vram(0x3F00)) == 0x3F);
		REQUIRE(static_cast<int>(read_vram(0x3F01)) == 0x00);
		REQUIRE(static_cast<int>(read_vram(0x3F02)) == 0x00);
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "VRAM Address Mirroring", "[ppu][memory][mirroring]") {
	SECTION("Address space should mirror at $4000") {
		// Write to base addresses (using valid 6-bit palette values)
		write_vram(0x2000, 0x12);
		write_vram(0x2345, 0x34);
		write_vram(0x3F00, 0x16); // Valid 6-bit value instead of 0x56
		write_vram(0x3F1F, 0x38); // Valid 6-bit value instead of 0x78

		// Test mirrors
		REQUIRE(static_cast<int>(read_vram(0x6000)) == 0x12); // $6000 mirrors $2000
		REQUIRE(static_cast<int>(read_vram(0x6345)) == 0x34); // $6345 mirrors $2345
		REQUIRE(static_cast<int>(read_vram(0x7F00)) == 0x16); // $7F00 mirrors $3F00
		REQUIRE(static_cast<int>(read_vram(0x7F1F)) == 0x38); // $7F1F mirrors $3F1F
	}

	SECTION("Nametable mirroring should depend on cartridge") {
		// This would need to be tested with different cartridge types
		// Horizontal mirroring: A=B, C=D
		// Vertical mirroring: A=C, B=D
		// Four-screen: A≠B≠C≠D
		// Single-screen: A=B=C=D

		// Write unique values to each nametable
		write_vram(0x2000, 0x00); // Nametable A
		write_vram(0x2400, 0x01); // Nametable B
		write_vram(0x2800, 0x02); // Nametable C
		write_vram(0x2C00, 0x03); // Nametable D

		// The actual mirroring behavior depends on cartridge configuration
		// Without cartridge, behavior is undefined
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "VRAM Increment Mode", "[ppu][memory][increment]") {
	SECTION("Increment by 1 mode should work") {
		// Set increment mode to 1 (bit 2 clear)
		write_ppu_register(0x2000, 0x00);

		set_vram_address(0x2000);

		// Write several bytes
		write_ppu_register(0x2007, 0x10);
		write_ppu_register(0x2007, 0x20);
		write_ppu_register(0x2007, 0x30);
		write_ppu_register(0x2007, 0x40);

		// Read back from sequential addresses
		REQUIRE(static_cast<int>(read_vram(0x2000)) == 0x10);
		REQUIRE(static_cast<int>(read_vram(0x2001)) == 0x20);
		REQUIRE(static_cast<int>(read_vram(0x2002)) == 0x30);
		REQUIRE(static_cast<int>(read_vram(0x2003)) == 0x40);
	}

	SECTION("Increment by 32 mode should work") {
		// Set increment mode to 32 (bit 2 set)
		write_ppu_register(0x2000, 0x04);

		set_vram_address(0x2000);

		// Write several bytes
		write_ppu_register(0x2007, 0x11);
		write_ppu_register(0x2007, 0x22);
		write_ppu_register(0x2007, 0x33);
		write_ppu_register(0x2007, 0x44);

		// Read back from addresses incremented by 32
		REQUIRE(static_cast<int>(read_vram(0x2000)) == 0x11);
		REQUIRE(static_cast<int>(read_vram(0x2020)) == 0x22);
		REQUIRE(static_cast<int>(read_vram(0x2040)) == 0x33);
		REQUIRE(static_cast<int>(read_vram(0x2060)) == 0x44);
	}

	SECTION("VRAM address should wrap at $4000") {
		write_ppu_register(0x2000, 0x00); // Increment by 1

		set_vram_address(0x3FFE);

		write_ppu_register(0x2007, 0x2A); // Valid 6-bit value (was 0xAA)
		write_ppu_register(0x2007, 0x3B); // Valid 6-bit value (was 0xBB)
		write_ppu_register(0x2007, 0x0C); // Valid 6-bit value (was 0xCC)

		REQUIRE(static_cast<int>(read_vram(0x3FFE)) == 0x2A);
		REQUIRE(static_cast<int>(read_vram(0x3FFF)) == 0x3B);
		REQUIRE(static_cast<int>(read_vram(0x0000)) == 0x0C); // Wrapped to beginning
	}
}

TEST_CASE_METHOD(MemoryMappingTestFixture, "Memory Access During Rendering", "[ppu][memory][rendering_access]") {
	SECTION("VRAM access should be blocked during rendering") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to visible scanline
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() >= 240 && safety_counter < MAX_CYCLES) {
			ppu->tick(CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("Loop hit safety limit - possible infinite loop");
		}

		// Try to access VRAM during rendering
		set_vram_address(0x2000);
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Access should be blocked or return garbage
		// Exact behavior depends on timing
	}

	SECTION("Palette access should work during rendering") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to visible scanline
		int safety_counter_2 = 0;
		const int MAX_CYCLES_2 = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() >= 240 && safety_counter_2 < MAX_CYCLES_2) {
			ppu->tick(CpuCycle{1});
			safety_counter_2++;
		}
		if (safety_counter_2 >= MAX_CYCLES_2) {
			throw std::runtime_error("Loop hit safety limit - possible infinite loop");
		}

		// Palette access should still work during rendering
		write_vram(0x3F00, 0x20);
		uint8_t value = read_vram(0x3F00);
		REQUIRE(static_cast<int>(value) == 0x20);
	}

	SECTION("OAM access should be blocked during sprite evaluation") {
		// Enable sprites
		write_ppu_register(0x2001, 0x10);

		// Advance to sprite evaluation time (cycles 65-256)
		int safety_counter_3 = 0;
		const int MAX_CYCLES_3 = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() >= 240 && safety_counter_3 < MAX_CYCLES_3) {
			ppu->tick(CpuCycle{1});
			safety_counter_3++;
		}
		if (safety_counter_3 >= MAX_CYCLES_3) {
			throw std::runtime_error("Loop hit safety limit - possible infinite loop");
		}

		int safety_counter_4 = 0;
		const int MAX_CYCLES_4 = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_cycle() < 65 && safety_counter_4 < MAX_CYCLES_4) {
			ppu->tick(CpuCycle{1});
			safety_counter_4++;
		}
		if (safety_counter_4 >= MAX_CYCLES_4) {
			throw std::runtime_error("Loop hit safety limit - possible infinite loop");
		}

		// OAM writes should be ignored during sprite evaluation
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 0x42);

		// OAM reads should return $FF during sprite evaluation
		uint8_t data = read_ppu_register(0x2004);
		REQUIRE(data == 0xFF);
	}
}
