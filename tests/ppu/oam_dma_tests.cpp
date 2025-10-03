#include "../catch2/catch_amalgamated.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include <memory>

using namespace nes;

class OAMDMATestFixture {
  public:
	OAMDMATestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		cartridge = std::make_shared<Cartridge>();
		apu = std::make_shared<APU>();

		// Connect components to bus
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);

		cpu = std::make_shared<CPU6502>(bus.get());
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
		cpu->reset();

		// Process the reset interrupt that was triggered by reset()
		// This ensures the CPU is in a clean state before tests start
		// Reset takes 7 cycles to complete
		cpu->tick(CpuCycle{10}); // Give extra cycles to ensure reset completes

		// Clear OAM
		clear_oam();
	}

	void write_cpu_memory(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_cpu_memory(uint16_t address) {
		return bus->read(address);
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void clear_oam() {
		write_ppu_register(0x2003, 0x00); // Set OAM address to 0
		for (int i = 0; i < 256; i++) {
			write_ppu_register(0x2004, 0x00); // Clear OAM
		}
	}

	void setup_test_data_in_ram(uint8_t page) {
		// Set up test pattern in specified RAM page
		uint16_t base_addr = static_cast<uint16_t>(page) << 8;
		for (uint16_t i = 0; i < 256; i++) {
			write_cpu_memory(base_addr + i, static_cast<uint8_t>(i));
		}
	}

	void verify_oam_contents(uint8_t expected_start_value) {
		// Verify OAM contains expected pattern
		write_ppu_register(0x2003, 0x00); // Reset OAM address
		for (int i = 0; i < 256; i++) {
			uint8_t oam_value = read_ppu_register(0x2004);
			uint8_t expected = static_cast<uint8_t>((expected_start_value + i) & 0xFF);
			REQUIRE(oam_value == expected);
		}
	}

	void wait_for_dma_completion() {
		// Wait for DMA to complete by advancing cycles
		int cycles_waited = 0;
		const int MAX_DMA_CYCLES = 600; // Safety limit

		while (bus->is_dma_active() && cycles_waited < MAX_DMA_CYCLES) {
			ppu->tick(CpuCycle{1}); // Advance PPU to process DMA
			cycles_waited++;
		}

		// Add a few extra cycles to ensure completion
		for (int i = 0; i < 10; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

	uint64_t get_cpu_cycle_count() {
		// For testing purposes, we'll use a simple cycle counter
		// In real implementation, this would track actual CPU cycles
		static uint64_t test_cycle_counter = 0;
		return test_cycle_counter++;
	}

	void trigger_oam_dma(uint8_t page) {
		write_ppu_register(0x4014, page);
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<Cartridge> cartridge;
	std::shared_ptr<APU> apu;
	std::shared_ptr<CPU6502> cpu;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(OAMDMATestFixture, "OAM DMA Basic Transfer", "[ppu][oam_dma][basic]") {
	SECTION("DMA should transfer 256 bytes from RAM to OAM") {
		// Set up test data in RAM page $02
		setup_test_data_in_ram(0x02);

		// Clear OAM to ensure clean test
		clear_oam();

		// Trigger OAM DMA from page $02
		trigger_oam_dma(0x02);

		// Wait for DMA to complete
		wait_for_dma_completion();

		// Verify OAM contains the test pattern
		verify_oam_contents(0x00);
	}

	SECTION("DMA should work with different RAM pages") {
		// Test DMA from page $03
		setup_test_data_in_ram(0x03);
		clear_oam();
		trigger_oam_dma(0x03);
		wait_for_dma_completion();
		verify_oam_contents(0x00);

		// Test DMA from page $01 (zero page area)
		setup_test_data_in_ram(0x01);
		clear_oam();
		trigger_oam_dma(0x01);
		wait_for_dma_completion();
		verify_oam_contents(0x00);
	}

	SECTION("DMA should start from current OAM address") {
		// Set up test data
		setup_test_data_in_ram(0x02);

		// Set OAM address to middle of buffer
		write_ppu_register(0x2003, 0x80);

		// Trigger DMA
		trigger_oam_dma(0x02);

		// Wait for DMA to complete
		wait_for_dma_completion();

		// DMA should start at OAM address $80 and wrap around
		write_ppu_register(0x2003, 0x80); // Reset to start position
		for (int i = 0; i < 256; i++) {
			uint8_t oam_value = read_ppu_register(0x2004);
			uint8_t expected = static_cast<uint8_t>(i);
			REQUIRE(oam_value == expected);
		}
	}
}

TEST_CASE_METHOD(OAMDMATestFixture, "OAM DMA Timing", "[ppu][oam_dma][timing]") {
	SECTION("DMA should take proper number of cycles") {
		setup_test_data_in_ram(0x02);

		// Test that CPU execution is properly halted during DMA
		// Set up a simple instruction at a known location
		write_cpu_memory(0x8000, 0xEA); // NOP instruction
		cpu->set_program_counter(0x8000);

		// Record initial state
		uint16_t initial_pc = cpu->get_program_counter();

		// Trigger DMA - this should halt the CPU
		trigger_oam_dma(0x02);

		// At this point, DMA should be active and CPU should be halted
		REQUIRE(bus->is_dma_active() == true);

		// Try to tick the CPU while DMA is active - it should remain halted
		cpu->tick(CpuCycle{1});
		REQUIRE(cpu->get_program_counter() == initial_pc); // PC shouldn't advance

		// Continue ticking until DMA completes (should take 513-514 cycles)
		int dma_cycles = 0;
		const int MAX_DMA_CYCLES = 600; // Safety limit

		while (bus->is_dma_active() && dma_cycles < MAX_DMA_CYCLES) {
			[[maybe_unused]] uint16_t pc_before_ticks = cpu->get_program_counter();

			ppu->tick(CpuCycle{1}); // Tick PPU to process DMA

			// Check if DMA completed during this PPU tick
			bool dma_still_active = bus->is_dma_active();

			cpu->tick(CpuCycle{1}); // CPU should remain halted OR resume if DMA just completed
			dma_cycles++;

			// PC should not advance during DMA, but may advance if DMA just completed
			if (dma_still_active) {
				// DMA still active - CPU should remain halted
				REQUIRE(cpu->get_program_counter() == initial_pc);
			} else {
				// DMA just completed - CPU may execute one instruction and advance PC
				// This is expected behavior when DMA finishes
			}
		}

		// DMA should complete within expected cycle count
		REQUIRE(dma_cycles >= 513);
		REQUIRE(dma_cycles <= 514);
		REQUIRE(bus->is_dma_active() == false);

		// After DMA completes, CPU should be able to execute normally
		// Note: PC may have already advanced during the loop when DMA completed
		if (cpu->get_program_counter() == initial_pc) {
			// PC hasn't advanced yet - execute one more instruction
			cpu->tick(CpuCycle{1});
		}
		// PC should now be past the initial instruction
		REQUIRE(cpu->get_program_counter() > initial_pc);
	}

	SECTION("CPU operations should be suspended during DMA") {
		setup_test_data_in_ram(0x02);

		// Set up a program that would normally execute multiple instructions
		// Use different address to avoid interference with first section
		write_cpu_memory(0x8100, 0xA9); // LDA #$42
		write_cpu_memory(0x8101, 0x42);
		write_cpu_memory(0x8102, 0xAA); // TAX
		write_cpu_memory(0x8103, 0xE8); // INX
		write_cpu_memory(0x8104, 0xEA); // NOP

		// Set PC to start of program
		cpu->set_program_counter(0x8100);
		uint8_t initial_accumulator = cpu->get_accumulator();
		uint8_t initial_x = cpu->get_x_register();

		// Trigger DMA
		trigger_oam_dma(0x02);

		// CPU should be halted - registers shouldn't change during DMA
		REQUIRE(bus->is_dma_active() == true);

		// Tick CPU multiple times while DMA is active
		for (int i = 0; i < 600; i++) { // Increased from 100 to 600 to handle full DMA duration (513+ cycles)
			[[maybe_unused]] uint16_t pc_before_ticks = cpu->get_program_counter();
			[[maybe_unused]] uint8_t acc_before_ticks = cpu->get_accumulator();
			[[maybe_unused]] uint8_t x_before_ticks = cpu->get_x_register();

			cpu->tick(CpuCycle{1});

			// Process PPU DMA
			ppu->tick(CpuCycle{1});

			// Check if DMA completed during this PPU tick
			bool dma_still_active = bus->is_dma_active();

			if (dma_still_active) {
				// DMA still active - CPU should remain halted
				REQUIRE(cpu->get_program_counter() == 0x8100);
				REQUIRE(cpu->get_accumulator() == initial_accumulator);
				REQUIRE(cpu->get_x_register() == initial_x);
			} else {
				// DMA just completed - CPU may have executed instructions
				// This is expected behavior when DMA finishes
				break;
			}
		}

		// DMA should eventually complete
		REQUIRE(bus->is_dma_active() == false);

		// Now CPU should be able to execute normally
		// Execute enough cycles to complete all instructions regardless of timing
		// LDA #$42 (2 cycles) + TAX (2 cycles) + INX (2 cycles) + NOP (2 cycles) = 8 cycles total
		while (cpu->get_program_counter() < 0x8105) {
			cpu->tick(CpuCycle{1});
		}

		// Verify instructions executed correctly after DMA
		REQUIRE(cpu->get_accumulator() == 0x42);
		REQUIRE(cpu->get_x_register() == 0x43);		   // 0x42 + 1 from INX
		REQUIRE(cpu->get_program_counter() == 0x8105); // Should advance past all instructions
	}
}

TEST_CASE_METHOD(OAMDMATestFixture, "OAM DMA Edge Cases", "[ppu][oam_dma][edge_cases]") {
	SECTION("DMA from mirrored RAM addresses") {
		// Set up data in RAM
		setup_test_data_in_ram(0x00);

		// DMA from mirrored address (should read same data)
		clear_oam();
		trigger_oam_dma(0x08); // $0800 mirrors to $0000
		wait_for_dma_completion();
		verify_oam_contents(0x00);

		clear_oam();
		trigger_oam_dma(0x10); // $1000 mirrors to $0000
		wait_for_dma_completion();
		verify_oam_contents(0x00);
	}

	SECTION("DMA should preserve OAM address after transfer") {
		setup_test_data_in_ram(0x02);

		// Set initial OAM address
		write_ppu_register(0x2003, 0x40);

		// Trigger DMA
		trigger_oam_dma(0x02);

		// Wait for DMA to complete
		wait_for_dma_completion();

		// OAM address should have wrapped around back to original + 256
		// Since 256 bytes were written, final address should be $40
		uint8_t final_oam_addr = read_ppu_register(0x2003);
		REQUIRE(final_oam_addr == 0x40);
	}

	SECTION("Multiple DMA transfers should work correctly") {
		// First transfer
		setup_test_data_in_ram(0x02);
		trigger_oam_dma(0x02);
		wait_for_dma_completion();

		// Set up different pattern for second transfer
		for (uint16_t i = 0x0300; i < 0x0400; i++) {
			write_cpu_memory(i, 0xFF - (i & 0xFF));
		}

		// Second transfer should overwrite first
		trigger_oam_dma(0x03);
		wait_for_dma_completion();

		// Verify second pattern
		write_ppu_register(0x2003, 0x00);
		for (int i = 0; i < 256; i++) {
			uint8_t oam_value = read_ppu_register(0x2004);
			uint8_t expected = 0xFF - i;
			REQUIRE(oam_value == expected);
		}
	}
}

TEST_CASE_METHOD(OAMDMATestFixture, "OAM DMA During Rendering", "[ppu][oam_dma][rendering]") {
	SECTION("DMA should work during VBlank") {
		setup_test_data_in_ram(0x02);

		// Advance PPU to VBlank period
		while (ppu->get_current_scanline() != 241) {
			ppu->tick(CpuCycle{1});
		}

		// DMA should work normally during VBlank
		clear_oam();
		trigger_oam_dma(0x02);
		wait_for_dma_completion();
		verify_oam_contents(0x00);
	}

	SECTION("DMA during active rendering should still work") {
		setup_test_data_in_ram(0x02);

		// Enable rendering
		write_ppu_register(0x2001, 0x1E); // Enable background and sprites

		// Advance to active rendering period
		while (ppu->get_current_scanline() >= 240) {
			ppu->tick(CpuCycle{1});
		}

		// DMA should still work (though may affect rendering)
		clear_oam();
		trigger_oam_dma(0x02);
		wait_for_dma_completion();

		// Advance to VBlank before verifying OAM contents
		// During rendering, OAM reads are restricted and return garbage
		while (ppu->get_current_scanline() < 241) {
			ppu->tick(CpuCycle{1});
		}

		verify_oam_contents(0x00);
	}
}

TEST_CASE_METHOD(OAMDMATestFixture, "OAM DMA Sprite Setup", "[ppu][oam_dma][sprites]") {
	SECTION("DMA should properly set up sprite data") {
		// Create sprite data in RAM
		uint16_t sprite_data_addr = 0x0200;

		// Sprite 0: Y=50, Tile=1, Attr=0, X=100
		write_cpu_memory(sprite_data_addr + 0, 50);	 // Y
		write_cpu_memory(sprite_data_addr + 1, 1);	 // Tile
		write_cpu_memory(sprite_data_addr + 2, 0);	 // Attributes
		write_cpu_memory(sprite_data_addr + 3, 100); // X

		// Sprite 1: Y=60, Tile=2, Attr=1, X=110
		write_cpu_memory(sprite_data_addr + 4, 60);	 // Y
		write_cpu_memory(sprite_data_addr + 5, 2);	 // Tile
		write_cpu_memory(sprite_data_addr + 6, 1);	 // Attributes
		write_cpu_memory(sprite_data_addr + 7, 110); // X

		// Fill rest with invalid sprites
		for (int i = 8; i < 256; i++) {
			write_cpu_memory(sprite_data_addr + i, 0xFF);
		}

		// Transfer to OAM
		trigger_oam_dma(0x02);

		// Wait for DMA to complete
		wait_for_dma_completion();

		// Verify sprite 0 data
		write_ppu_register(0x2003, 0x00);
		REQUIRE(read_ppu_register(0x2004) == 50);  // Y
		REQUIRE(read_ppu_register(0x2004) == 1);   // Tile
		REQUIRE(read_ppu_register(0x2004) == 0);   // Attr
		REQUIRE(read_ppu_register(0x2004) == 100); // X

		// Verify sprite 1 data
		REQUIRE(read_ppu_register(0x2004) == 60);  // Y
		REQUIRE(read_ppu_register(0x2004) == 2);   // Tile
		REQUIRE(read_ppu_register(0x2004) == 1);   // Attr
		REQUIRE(read_ppu_register(0x2004) == 110); // X
	}
}

TEST_CASE_METHOD(OAMDMATestFixture, "DMA Hardware Accuracy", "[ppu][oam_dma][hardware_accuracy]") {
	SECTION("DMA active status during transfer") {
		setup_test_data_in_ram(0x02);

		// Start DMA
		bus->write(0x4014, 0x02);

		// Check DMA is active immediately after write
		REQUIRE(bus->is_dma_active() == true);

		// Tick through most of the transfer
		for (int i = 0; i < 500; i++) {
			ppu->tick(CpuCycle{1});
			REQUIRE(bus->is_dma_active() == true);
		}

		// Complete the transfer
		for (int i = 0; i < 20; i++) {
			ppu->tick(CpuCycle{1});
		}

		// DMA should be complete
		REQUIRE(bus->is_dma_active() == false);
	}

	SECTION("Cycle-by-cycle data transfer accuracy") {
		// Fill source memory with incrementing pattern
		for (int i = 0; i < 256; i++) {
			write_cpu_memory(0x0200 + i, static_cast<Byte>(i));
		}

		// Ensure OAM starts clear
		for (int i = 0; i < 256; i++) {
			write_ppu_register(0x2003, static_cast<Byte>(i));
			write_ppu_register(0x2004, 0x00);
		}

		// Start DMA
		bus->write(0x4014, 0x02);

		// Allow DMA to complete
		while (bus->is_dma_active()) {
			ppu->tick(CpuCycle{1});
		}

		// Verify all 256 bytes transferred correctly
		for (int i = 0; i < 256; i++) {
			write_ppu_register(0x2003, static_cast<Byte>(i));
			Byte oam_data = read_ppu_register(0x2004);
			REQUIRE(static_cast<int>(oam_data) == i);
		}
	}
}
