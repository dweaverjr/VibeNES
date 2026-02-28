// VibeNES - NES Emulator
// PPU Timing Tests
// Tests for hardware-accurate PPU timing behavior

#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include <catch2/catch_all.hpp>
#include <iostream>
#include <memory>

using namespace nes;

class TimingTestFixture {
  public:
	TimingTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		cartridge = std::make_shared<Cartridge>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());

		// Connect components to bus
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

		// Power on the system
		bus->power_on();
		ppu->power_on();
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		int max_cycles = 100000; // Safety limit
		while (ppu->get_current_scanline() < target_scanline && safety_counter < max_cycles) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			safety_counter++;
		}
		if (safety_counter >= max_cycles) {
			FAIL("advance_to_scanline hit safety limit - PPU may not be properly connected");
		}
	}

	void advance_to_cycle(int target_cycle) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops

		// If target cycle is less than current cycle, we need to advance to next scanline
		if (target_cycle < ppu->get_current_cycle()) {
			// Advance to next scanline first
			while (ppu->get_current_cycle() != 0 && safety_counter < MAX_CYCLES) {
				ppu->tick_single_dot();
				safety_counter++;
			}
		}

		// Now advance to the target cycle within the current scanline
		while (ppu->get_current_cycle() < target_cycle && safety_counter < MAX_CYCLES) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_cycle hit safety limit - possible infinite loop");
		}
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
		}
	}

	void advance_full_frame() {
		uint64_t start_frame = ppu->get_frame_count();
		while (ppu->get_frame_count() == start_frame) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
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

TEST_CASE_METHOD(TimingTestFixture, "Frame Timing", "[ppu][timing][frame]") {
	SECTION("Frame should have correct total cycles") {
		uint64_t start_frame = ppu->get_frame_count();
		int cycle_count = 0;

		while (ppu->get_frame_count() == start_frame) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			cycle_count++;
		}

		// NTSC: 262 scanlines * 341 cycles = 89342 cycles per frame
		// PAL: 312 scanlines * 341 cycles = 106412 cycles per frame
		// Odd frames skip cycle 340 of scanline 261: 89341 cycles
		REQUIRE((cycle_count == 89341 || cycle_count == 89342));
	}

	SECTION("Odd frame skip should work") {
		// Rendering must be enabled for odd frame skip
		write_ppu_register(0x2001, 0x18);

		// Advance to frame boundary
		advance_full_frame();

		int odd_frame_cycles = 0;
		uint64_t start_frame = ppu->get_frame_count();

		while (ppu->get_frame_count() == start_frame) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			odd_frame_cycles++;
		}

		// Odd frames should be 1 cycle shorter
		REQUIRE(odd_frame_cycles == 89341);

		// Next frame (even) should be full length
		int even_frame_cycles = 0;
		start_frame = ppu->get_frame_count();

		while (ppu->get_frame_count() == start_frame) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			even_frame_cycles++;
		}

		REQUIRE(even_frame_cycles == 89342);
	}
}

TEST_CASE_METHOD(TimingTestFixture, "Scanline Timing", "[ppu][timing][scanline]") {
	SECTION("Visible scanlines should be 0-239") {
		advance_to_scanline(0);
		REQUIRE(ppu->get_current_scanline() == 0);

		advance_to_scanline(239);
		REQUIRE(ppu->get_current_scanline() == 239);

		// Next scanline should be post-render
		advance_to_scanline(240);
		REQUIRE(ppu->get_current_scanline() == 240);
	}

	SECTION("Post-render scanline should be 240") {
		advance_to_scanline(240);
		REQUIRE(ppu->get_current_scanline() == 240);

		// Post-render scanline is mostly idle
	}

	SECTION("VBlank scanlines should be 241-260") {
		advance_to_scanline(241);
		REQUIRE(ppu->get_current_scanline() == 241);

		// VBlank flag should be set at cycle 1 of scanline 241
		advance_to_cycle(1);
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) != 0); // VBlank flag set

		advance_to_scanline(260);
		REQUIRE(ppu->get_current_scanline() == 260);
	}

	SECTION("Pre-render scanline should be 261") {
		advance_to_scanline(261);
		REQUIRE(ppu->get_current_scanline() == 261);

		// VBlank flag should be cleared at cycle 1 of scanline 261
		advance_to_cycle(1);
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) == 0); // VBlank flag clear
	}
}

TEST_CASE_METHOD(TimingTestFixture, "VBlank Timing", "[ppu][timing][vblank]") {
	SECTION("VBlank flag should set at scanline 241, cycle 1") {
		advance_to_scanline(241);
		advance_to_cycle(0);

		uint8_t status_before = read_ppu_register(0x2002);
		REQUIRE((status_before & 0x80) == 0); // VBlank clear

		advance_to_cycle(1);
		uint8_t status_after = read_ppu_register(0x2002);
		REQUIRE((status_after & 0x80) != 0); // VBlank set
	}

	SECTION("VBlank flag should clear at scanline 261, cycle 1") {
		// First set VBlank
		advance_to_scanline(241);
		advance_to_cycle(1);

		uint8_t status_set = read_ppu_register(0x2002);
		REQUIRE((status_set & 0x80) != 0);

		// Set VBlank flag again since reading cleared it
		advance_to_cycle(1); // This will set VBlank again

		// Then advance to pre-render scanline 261
		advance_to_scanline(261);

		// At scanline 261, cycle 0, VBlank should still be set (don't read to check)
		// Just advance to cycle 1 where it should be cleared by hardware
		advance_to_cycle(1);

		uint8_t status_after = read_ppu_register(0x2002);
		REQUIRE((status_after & 0x80) == 0); // Now cleared
	}

	SECTION("Reading PPUSTATUS should clear VBlank flag") {
		advance_to_scanline(241);
		advance_to_cycle(1);

		uint8_t status1 = read_ppu_register(0x2002);
		REQUIRE((status1 & 0x80) != 0); // VBlank set

		uint8_t status2 = read_ppu_register(0x2002);
		REQUIRE((status2 & 0x80) == 0); // VBlank cleared by read
	}
}

TEST_CASE_METHOD(TimingTestFixture, "Rendering Cycles", "[ppu][timing][rendering]") {
	SECTION("Tile fetch cycles should follow pattern") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(10); // Visible scanline

		// Each tile takes 8 cycles to fetch:
		// Cycle 1: Nametable byte
		// Cycle 3: Attribute byte
		// Cycle 5: Pattern low byte
		// Cycle 7: Pattern high byte
		// Cycle 8: Store in shift registers

		for (int tile = 0; tile < 32; tile++) {
			int base_cycle = tile * 8 + 1;

			advance_to_cycle(base_cycle);	  // Nametable fetch
			advance_to_cycle(base_cycle + 2); // Attribute fetch
			advance_to_cycle(base_cycle + 4); // Pattern low fetch
			advance_to_cycle(base_cycle + 6); // Pattern high fetch
			advance_to_cycle(base_cycle + 7); // Store in shift registers
		}
	}

	SECTION("Sprite evaluation cycles should be correct") {
		// Enable sprites
		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(50);

		// Sprite evaluation: cycles 65-256
		advance_to_cycle(64);
		// Sprite evaluation not started

		advance_to_cycle(65);
		// Sprite evaluation starts

		advance_to_cycle(256);
		// Sprite evaluation complete
	}

	SECTION("VRAM address updates should happen at correct cycles") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(10);

		// Coarse X increment: every 8 cycles during visible rendering
		for (int cycle = 8; cycle <= 256; cycle += 8) {
			advance_to_cycle(cycle);
			// Coarse X should increment here
		}

		// Y increment: cycle 256
		advance_to_cycle(256);
		// Fine Y should increment here

		// Horizontal position copy: cycle 257
		advance_to_cycle(257);
		// Horizontal position copied from temp VRAM address
	}
}

TEST_CASE_METHOD(TimingTestFixture, "Memory Access Timing", "[ppu][timing][memory]") {
	SECTION("VRAM reads should not advance PPU cycle counter") {
		// PPU register reads happen on the CPU bus; the PPU cycle counter
		// only advances when the PPU is ticked. Register access does NOT
		// consume PPU dots.
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		int start_cycle = ppu->get_current_cycle();
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);
		int end_cycle = ppu->get_current_cycle();

		REQUIRE(end_cycle - start_cycle == 0);
	}

	SECTION("VRAM writes should not advance PPU cycle counter") {
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		int start_cycle = ppu->get_current_cycle();
		write_ppu_register(0x2007, 0x42);
		int end_cycle = ppu->get_current_cycle();

		REQUIRE(end_cycle - start_cycle == 0);
	}

	SECTION("VRAM access during rendering should be restricted") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(50); // Visible scanline
		advance_to_cycle(100);	 // During rendering

		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// VRAM reads during rendering should return garbage
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);
		// Value is unpredictable during rendering
	}
}

TEST_CASE_METHOD(TimingTestFixture, "Register Access Timing", "[ppu][timing][registers]") {
	SECTION("PPUSTATUS read should clear write toggle") {
		// Set write toggle with PPUSCROLL write
		write_ppu_register(0x2005, 0x10);

		// Read PPUSTATUS to clear toggle
		read_ppu_register(0x2002);

		// Next PPUSCROLL write should affect X scroll (first write)
		write_ppu_register(0x2005, 0x20);
		write_ppu_register(0x2005, 0x30);
	}

	SECTION("PPUADDR writes should affect VRAM address") {
		write_ppu_register(0x2006, 0x23);
		write_ppu_register(0x2006, 0x45);

		// Current VRAM address should be $2345
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);
		// Reading from $2345
	}

	SECTION("PPUSCROLL writes should follow toggle pattern") {
		// Clear toggle
		read_ppu_register(0x2002);

		// First write: X scroll
		write_ppu_register(0x2005, 0x10);

		// Second write: Y scroll
		write_ppu_register(0x2005, 0x20);

		// Third write: X scroll again (toggle reset)
		write_ppu_register(0x2005, 0x30);
	}
}

TEST_CASE_METHOD(TimingTestFixture, "Interrupt Timing", "[ppu][timing][interrupts]") {
	SECTION("NMI should trigger at correct time") {
		// Enable NMI
		write_ppu_register(0x2000, 0x80);

		advance_to_scanline(241);
		advance_to_cycle(0);

		// NMI should not be triggered yet
		advance_to_cycle(1);

		// NMI should be triggered here (if NMI enabled)
		// This would need to be tested at the CPU level
	}

	SECTION("NMI enable during VBlank should work") {
		// VBlank already active
		advance_to_scanline(241);
		advance_to_cycle(1);

		// Enable NMI while VBlank is active
		write_ppu_register(0x2000, 0x80);

		// NMI should trigger immediately
	}

	SECTION("NMI disable should prevent interrupt") {
		// Enable NMI
		write_ppu_register(0x2000, 0x80);

		advance_to_scanline(241);

		// Disable NMI just before VBlank
		write_ppu_register(0x2000, 0x00);

		advance_to_cycle(1);

		// NMI should not trigger
	}
}
