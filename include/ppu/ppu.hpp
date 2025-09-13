#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include "ppu/ppu_memory.hpp"
#include "ppu/ppu_registers.hpp"
#include <array>
#include <memory>

namespace nes {

// Forward declarations
class SystemBus;
class Cartridge;
class CPU6502;

/// Scanline phases for PPU timing
enum class ScanlinePhase {
	VISIBLE,	 // Scanlines 0-239: Visible scanlines
	POST_RENDER, // Scanline 240: Post-render scanline
	VBLANK,		 // Scanlines 241-260: Vertical blank
	PRE_RENDER	 // Scanline 261: Pre-render scanline
};

/// NES PPU (Picture Processing Unit) 2C02
/// Generates video output at 256x240 resolution, 60Hz refresh rate
/// Runs at 3x CPU clock speed (5.37 MHz)
class PPU : public Component {
  public:
	PPU();
	~PPU() override = default;

	// Component interface implementation
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	const char *get_name() const noexcept override {
		return "PPU";
	}

	// CPU interface - memory-mapped registers ($2000-$2007, mirrored through $3FFF)
	uint8_t read_register(uint16_t address);
	void write_register(uint16_t address, uint8_t value);

	// Frame buffer access
	bool is_frame_ready() const {
		return frame_ready_;
	}
	const uint32_t *get_frame_buffer() const {
		return frame_buffer_.data();
	}
	void clear_frame_ready() {
		frame_ready_ = false;
	}

	// Connect to system bus for NMI generation
	void connect_bus(SystemBus *bus) {
		bus_ = bus;
	}

	// Connect to CPU for NMI interrupt generation
	void connect_cpu(CPU6502 *cpu) {
		cpu_ = cpu;
	}

	// Connect to cartridge for CHR ROM/RAM access
	void connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
		cartridge_ = cartridge;
	}

	// Debug/inspection methods
	uint16_t get_current_scanline() const {
		return current_scanline_;
	}
	uint16_t get_current_cycle() const {
		return current_cycle_;
	}
	uint64_t get_frame_count() const {
		return frame_counter_;
	}

	// Register inspection (for debugging)
	uint8_t get_control_register() const {
		return control_register_;
	}
	uint8_t get_mask_register() const {
		return mask_register_;
	}
	uint8_t get_status_register() const {
		return status_register_;
	}

	// Memory access for debugging
	const PPUMemory &get_memory() const {
		return memory_;
	}

	// CHR ROM access (for pattern table visualization)
	uint8_t read_chr_rom(uint16_t address) const;

  private:
	// Timing state
	uint16_t current_cycle_;	// Current PPU cycle (0-340)
	uint16_t current_scanline_; // Current scanline (0-261)
	uint64_t frame_counter_;	// Total frames rendered
	bool frame_ready_;			// Flag indicating new frame is ready

	// PPU registers ($2000-$2007)
	uint8_t control_register_; // $2000 PPUCTRL
	uint8_t mask_register_;	   // $2001 PPUMASK
	uint8_t status_register_;  // $2002 PPUSTATUS
	uint8_t oam_address_;	   // $2003 OAMADDR

	// Internal latches and state
	uint16_t vram_address_;		 // Current VRAM address (v)
	uint16_t temp_vram_address_; // Temporary VRAM address (t)
	uint8_t fine_x_scroll_;		 // Fine X scroll (3 bits)
	bool write_toggle_;			 // First/second write toggle (w)
	uint8_t read_buffer_;		 // PPU data read buffer

	// Memory management
	PPUMemory memory_;

	// Frame buffer (256x240 pixels, 32-bit RGBA)
	std::array<uint32_t, 256 * 240> frame_buffer_;

	// Sprite evaluation state (for current scanline)
	struct ScanlineSprite {
		Sprite sprite_data;
		uint8_t sprite_index;	   // Original sprite index (for sprite 0 detection)
		uint8_t pattern_data_low;  // Low bit plane for current row
		uint8_t pattern_data_high; // High bit plane for current row
	};
	std::array<ScanlineSprite, 8> scanline_sprites_; // Max 8 sprites per scanline
	uint8_t sprite_count_current_scanline_;			 // Number of sprites on current scanline
	bool sprite_0_on_scanline_;						 // True if sprite 0 is on current scanline

	// External connections
	SystemBus *bus_;					   // For NMI generation
	CPU6502 *cpu_;						   // For triggering NMI interrupts
	std::shared_ptr<Cartridge> cartridge_; // For CHR ROM/RAM access

	// Internal tick function - called once per PPU cycle
	void tick_internal();

	// Scanline processing
	void process_visible_scanline();
	void process_post_render_scanline();
	void process_vblank_scanline();
	void process_pre_render_scanline();

	// Register handlers
	uint8_t read_ppustatus();
	uint8_t read_oamdata();
	uint8_t read_ppudata();

	void write_ppuctrl(uint8_t value);
	void write_ppumask(uint8_t value);
	void write_oamaddr(uint8_t value);
	void write_oamdata(uint8_t value);
	void write_ppuscroll(uint8_t value);
	void write_ppuaddr(uint8_t value);
	void write_ppudata(uint8_t value);

	// Memory access helpers
	uint8_t read_ppu_memory(uint16_t address);
	void write_ppu_memory(uint16_t address, uint8_t value);

	// Rendering helpers
	void render_pixel();
	void clear_frame_buffer();

	// Background rendering (Phase 2)
	void render_background_pixel();
	uint8_t get_background_pixel_at_current_position();
	uint8_t fetch_nametable_byte(uint16_t nametable_addr);
	uint8_t fetch_attribute_byte(uint16_t nametable_addr);
	uint16_t fetch_pattern_data(uint8_t tile_index, uint8_t fine_y, bool background_table);
	uint8_t get_background_palette_index(uint16_t pattern_data, uint8_t attribute, uint8_t fine_x);
	uint32_t get_palette_color(uint8_t palette_index);

	// Sprite rendering (Phase 3)
	void render_sprite_pixel();
	uint8_t get_sprite_pixel_at_current_position(bool &sprite_priority);
	void evaluate_sprites_for_scanline();
	uint8_t fetch_sprite_pattern_data(const Sprite &sprite, uint8_t sprite_y, bool sprite_table);
	uint8_t fetch_sprite_pattern_data_raw(uint8_t tile_index, uint8_t fine_y, bool sprite_table);
	uint8_t get_sprite_palette_index(uint8_t pattern_data, uint8_t palette, uint8_t fine_x);
	bool check_sprite_0_hit(uint8_t bg_pixel, uint8_t sprite_pixel, uint8_t x_pos);
	void render_combined_pixel(uint8_t bg_pixel, uint8_t sprite_pixel, bool sprite_priority, uint8_t x_pos,
							   uint8_t y_pos);

	// Advanced scrolling (Phase 4)
	void update_vram_address_x();
	void update_vram_address_y();
	void copy_horizontal_scroll();
	void copy_vertical_scroll();
	void increment_coarse_x();
	void increment_fine_y();
	uint16_t get_current_nametable_address();
	uint16_t get_current_attribute_address();
	uint8_t get_fine_y_scroll();

	// NMI generation
	void check_nmi();

	// Internal state helpers
	ScanlinePhase get_current_phase() const;
	bool is_rendering_enabled() const;
	bool is_background_enabled() const;
	bool is_sprites_enabled() const;
};

/// PPU timing constants
namespace PPUTiming {
constexpr uint16_t CYCLES_PER_SCANLINE = 341;
constexpr uint16_t VISIBLE_SCANLINES = 240;
constexpr uint16_t POST_RENDER_SCANLINE = 240;
constexpr uint16_t VBLANK_START_SCANLINE = 241;
constexpr uint16_t VBLANK_END_SCANLINE = 260;
constexpr uint16_t PRE_RENDER_SCANLINE = 261;
constexpr uint16_t TOTAL_SCANLINES = 262;

constexpr uint16_t VISIBLE_PIXELS = 256;
constexpr uint16_t HBLANK_START = 256;
constexpr uint16_t HBLANK_END = 340;

constexpr uint16_t VBLANK_SET_CYCLE = 1;   // VBlank flag set at cycle 1 of scanline 241
constexpr uint16_t VBLANK_CLEAR_CYCLE = 1; // VBlank flag cleared at cycle 1 of pre-render
} // namespace PPUTiming

} // namespace nes
