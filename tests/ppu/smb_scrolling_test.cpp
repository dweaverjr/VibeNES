// Test file to reproduce Super Mario Bros horizontal scrolling bug
// This simulates the actual scrolling behavior seen in the game

#include "../catch2/catch_amalgamated.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"

using namespace nes;

namespace {

class SMBScrollingFixture {
  public:
	SMBScrollingFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());
		ppu = std::make_shared<PPU>();

		// Create cartridge with vertical mirroring (for horizontal scrolling)
		RomData rom_data;
		rom_data.mapper_id = 0;
		rom_data.prg_rom_pages = 2;
		rom_data.chr_rom_pages = 1;
		rom_data.vertical_mirroring = true; // Critical for SMB horizontal scrolling
		rom_data.battery_backed_ram = false;
		rom_data.trainer_present = false;
		rom_data.four_screen_vram = false;
		rom_data.prg_rom.resize(32768, 0xEA);
		rom_data.chr_rom.resize(8192, 0x00);
		rom_data.filename = "smb_test.nes";
		rom_data.valid = true;

		cartridge = std::make_shared<Cartridge>();
		cartridge->load_from_rom_data(rom_data);

		// Connect components
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);
		bus->connect_cpu(cpu);
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);
		ppu->connect_cartridge(cartridge);
		ppu->connect_cpu(cpu.get());

		// Power on
		bus->power_on();
		ppu->power_on();
		cpu->tick(CpuCycle{10});
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void write_vram(uint16_t address, uint8_t value) {
		read_ppu_register(0x2002); // Reset latch
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
		write_ppu_register(0x2007, value);
	}

	void set_scroll(uint8_t x, uint8_t y) {
		read_ppu_register(0x2002); // Reset latch
		write_ppu_register(0x2005, x);
		write_ppu_register(0x2005, y);
	}

	void advance_to_scanline(int target) {
		int safety = 100000;
		while (ppu->get_current_scanline() != target && safety-- > 0) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_to_cycle(int target) {
		int safety = 10000;
		while (ppu->get_current_cycle() < target && safety-- > 0) {
			ppu->tick(CpuCycle{1});
		}
	}

	void render_full_frame() {
		uint64_t start_frame = ppu->get_frame_count();
		int safety = 1000000;
		while (ppu->get_frame_count() <= start_frame && safety-- > 0) {
			ppu->tick(CpuCycle{1});
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

} // namespace

TEST_CASE_METHOD(SMBScrollingFixture, "SMB Scrolling - Dynamic Scroll Updates", "[ppu][smb][scrolling]") {
	SECTION("Simulating Mario walking right with scroll updates each frame") {
		// Fill left nametable with one pattern, right with another
		for (uint16_t i = 0; i < 960; ++i) {
			write_vram(0x2000 + i, 0xAA); // Left nametable
			write_vram(0x2400 + i, 0xBB); // Right nametable
		}

		// Enable rendering
		write_ppu_register(0x2000, 0x00); // PPUCTRL: nametable 0, no scroll
		write_ppu_register(0x2001, 0x18); // PPUMASK: show background and sprites

		// Simulate multiple frames with increasing scroll
		for (int scroll_x = 0; scroll_x < 256; scroll_x += 8) {
			// Wait for VBlank
			advance_to_scanline(241);

			// Update scroll during VBlank (like SMB does)
			set_scroll(static_cast<uint8_t>(scroll_x), 0);

			// Render the frame
			render_full_frame();

			// Check that we're reading from the correct nametable
			// When scroll_x < 256, we should only see tiles from left nametable (0xAA)
			// The right nametable (0xBB) should not bleed into the viewport
			auto debug_state = ppu->get_debug_state();

			INFO("Scroll X: " << scroll_x);
			INFO("VRAM Address: 0x" << std::hex << debug_state.vram_address);
			INFO("Temp VRAM Address: 0x" << std::hex << debug_state.temp_vram_address);
			INFO("Fine X: " << static_cast<int>(debug_state.fine_x_scroll));

			// Verify temp_vram_address has correct coarse X
			uint16_t expected_coarse_x = scroll_x >> 3;
			uint16_t actual_coarse_x = debug_state.temp_vram_address & 0x001F;
			REQUIRE(actual_coarse_x == expected_coarse_x);

			// Verify fine X is correct
			uint8_t expected_fine_x = scroll_x & 0x07;
			REQUIRE(debug_state.fine_x_scroll == expected_fine_x);
		}
	}

	SECTION("Crossing nametable boundary at scroll=256") {
		// Fill both nametables with distinct patterns
		for (uint16_t i = 0; i < 960; ++i) {
			write_vram(0x2000 + i, 0x11); // Left NT
			write_vram(0x2400 + i, 0x22); // Right NT
		}

		// Enable rendering with nametable 0 selected
		write_ppu_register(0x2000, 0x00); // PPUCTRL: nametable 0
		write_ppu_register(0x2001, 0x18); // PPUMASK: rendering enabled

		// Set scroll to exactly 256 pixels (crossing to right nametable)
		advance_to_scanline(241);
		set_scroll(0, 0); // Reset scroll to 0

		// Now scroll to 255 (last pixel of left nametable)
		advance_to_scanline(241);
		set_scroll(255, 0);
		render_full_frame();

		// At scroll=255, we should see the last tile of left NT and first tile of right NT
		auto debug_state = ppu->get_debug_state();
		INFO("At scroll=255:");
		INFO("Coarse X: " << (debug_state.temp_vram_address & 0x001F));
		INFO("Fine X: " << static_cast<int>(debug_state.fine_x_scroll));
		INFO("Nametable bits: 0x" << std::hex << ((debug_state.temp_vram_address >> 10) & 0x03));

		// Coarse X should be 31, fine X should be 7
		REQUIRE((debug_state.temp_vram_address & 0x001F) == 31);
		REQUIRE(debug_state.fine_x_scroll == 7);
		REQUIRE(((debug_state.temp_vram_address >> 10) & 0x01) == 0); // Still in left nametable
	}

	SECTION("PPUCTRL nametable select with scrolling") {
		// SMB uses PPUCTRL to change base nametable, not just scrolling
		for (uint16_t i = 0; i < 960; ++i) {
			write_vram(0x2000 + i, 0x33); // NT 0
			write_vram(0x2400 + i, 0x44); // NT 1
		}

		// Test with PPUCTRL selecting right nametable
		write_ppu_register(0x2000, 0x01); // PPUCTRL: nametable 1 (bit 0 set)
		write_ppu_register(0x2001, 0x18);

		// Check immediately after PPUCTRL write
		auto debug_state_after_ctrl = ppu->get_debug_state();
		INFO("After PPUCTRL write:");
		INFO("Temp VRAM Address: 0x" << std::hex << debug_state_after_ctrl.temp_vram_address);
		INFO("Nametable bits: " << ((debug_state_after_ctrl.temp_vram_address >> 10) & 0x03));

		// PPUCTRL bit 0 should set temp_vram_address bit 10
		REQUIRE(((debug_state_after_ctrl.temp_vram_address >> 10) & 0x01) == 1);

		advance_to_scanline(241);
		set_scroll(0, 0);

		// Check after scroll write
		auto debug_state_after_scroll = ppu->get_debug_state();
		INFO("After PPUSCROLL write:");
		INFO("Temp VRAM Address: 0x" << std::hex << debug_state_after_scroll.temp_vram_address);
		INFO("Nametable bits: " << ((debug_state_after_scroll.temp_vram_address >> 10) & 0x03));

		// PPUSCROLL should preserve nametable bit from PPUCTRL
		REQUIRE(((debug_state_after_scroll.temp_vram_address >> 10) & 0x01) == 1);

		render_full_frame();

		auto debug_state = ppu->get_debug_state();
		INFO("After render:");
		INFO("Temp VRAM Address: 0x" << std::hex << debug_state.temp_vram_address);
		INFO("Nametable select bits: " << ((debug_state.temp_vram_address >> 10) & 0x03));

		// With PPUCTRL bit 0 set, nametable select bit 10 should be 1
		REQUIRE(((debug_state.temp_vram_address >> 10) & 0x01) == 1);
	}
}
