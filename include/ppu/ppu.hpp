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
	void tick_single_dot(); // Advance PPU by exactly 1 dot - for testing
	void reset() override;
	void power_on() override;
	const char *get_name() const noexcept override {
		return "PPU 2C02";
	}

	// CPU interface - memory-mapped registers ($2000-$2007, mirrored through $3FFF)
	uint8_t read_register(uint16_t address);
	void write_register(uint16_t address, uint8_t value);

	// Non-intrusive register peek for debugging (no side effects)
	uint8_t peek_register(uint16_t address) const;

	// OAM DMA interface ($4014)
	void write_oam_dma(uint8_t page);
	void write_oam_direct(uint8_t offset, uint8_t value);
	bool is_oam_dma_active() const {
		return oam_dma_active_ || oam_dma_pending_;
	}
	void perform_oam_dma_cycle();

	// OAM access for debugging/testing
	uint8_t read_oam(uint8_t address) const {
		return oam_memory_[address];
	}
	void write_oam(uint8_t address, uint8_t value) {
		oam_memory_[address] = value;
	}

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
	void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

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
	ScanlinePhase get_current_phase() const;

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

	struct DebugState {
		uint16_t cycle;
		uint16_t scanline;
		uint16_t vram_address;
		uint16_t temp_vram_address;
		uint8_t fine_x_scroll;
		uint8_t control_register;
		uint8_t mask_register;
		uint8_t status_register;
		uint16_t bg_pattern_low_shift;
		uint16_t bg_pattern_high_shift;
		uint16_t bg_attribute_low_shift;
		uint16_t bg_attribute_high_shift;
		uint8_t next_tile_id;
		uint8_t next_tile_attribute;
		uint8_t next_tile_pattern_low;
		uint8_t next_tile_pattern_high;
		uint8_t fetch_cycle;
		uint8_t current_tile_id;
		uint8_t current_attribute;
		uint8_t current_pattern_low;
		uint8_t current_pattern_high;
	};
	[[nodiscard]] DebugState get_debug_state() const;

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset);

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
	uint16_t vram_address_;				// Current VRAM address (v)
	uint16_t temp_vram_address_;		// Temporary VRAM address (t)
	uint8_t fine_x_scroll_;				// Fine X scroll (3 bits)
	bool write_toggle_;					// First/second write toggle (w)
	uint8_t read_buffer_;				// PPU data read buffer
	bool vram_wrap_read_pending_;		// Pending wrapped-read override flag
	uint16_t vram_wrap_target_address_; // Address that should return the latched value after wrap
	uint8_t vram_wrap_latched_value_;	// Latched value to expose on first read after wrap

	// OAM (Object Attribute Memory) Management
	std::array<uint8_t, 256> oam_memory_;		  // Primary OAM (64 sprites × 4 bytes)
	std::array<uint8_t, 32> secondary_oam_;		  // Secondary OAM for scanline sprites (8 sprites × 4 bytes)
	std::array<uint8_t, 8> secondary_oam_source_; // Which OAM sprite (0-63) filled each secondary OAM slot
	bool oam_dma_active_;						  // OAM DMA in progress ($4014)
	uint16_t oam_dma_address_;					  // OAM DMA source address
	uint16_t oam_dma_cycle_;					  // OAM DMA cycle counter (0-513)
	uint8_t oam_dma_subcycle_;					  // PPU subcycle counter for CPU timing (0-2)
	bool oam_dma_pending_;						  // OAM DMA requested but not started
	uint8_t oam_dma_data_latch_;				  // Latch holding data between DMA read/write phases

	// Hardware Timing State
	bool odd_frame_;					   // Tracks odd/even frames for cycle skip
	uint8_t nmi_delay_;					   // NMI generation delay cycles
	bool suppress_vbl_;					   // VBlank suppression flag
	bool rendering_disabled_mid_scanline_; // Track rendering disable timing
	bool was_rendering_enabled_;		   // Previous frame's rendering state (for mid-scanline disable detection)

	// Bus State and Open Bus Behavior
	uint8_t ppu_data_bus_;				   // PPU data bus for open bus behavior
	uint8_t io_db_;						   // I/O data bus latch
	bool vram_address_corruption_pending_; // VRAM address corruption flag

	// MMC3 A12 line tracking for IRQ counter
	// Real hardware clocks the IRQ counter on a rising edge of A12 only
	// when A12 has been low for at least ~15 PPU cycles (low-time filter).
	bool last_a12_state_;								 // Previous state of A12 line (for edge detection)
	uint32_t ppu_dot_counter_;							 // Monotonic PPU dot counter (wraps, used for deltas)
	uint32_t a12_last_high_dot_;						 // ppu_dot_counter_ value when A12 was last high
	static constexpr uint32_t A12_FILTER_THRESHOLD = 15; // PPU cycles A12 must be low

	// Memory management
	PPUMemory memory_;

	// Frame buffer (256x240 pixels, 32-bit RGBA)
	std::array<uint32_t, 256 * 240> frame_buffer_;

	// Background shift registers (2-tile lookahead like real hardware)
	struct BackgroundShiftRegisters {
		uint16_t pattern_low_shift;	   // Low bit plane shift register
		uint16_t pattern_high_shift;   // High bit plane shift register
		uint16_t attribute_low_shift;  // Attribute low bit shift register
		uint16_t attribute_high_shift; // Attribute high bit shift register

		// Latches for next tile data
		uint8_t next_tile_id;
		uint8_t next_tile_attribute;
		uint8_t next_tile_pattern_low;
		uint8_t next_tile_pattern_high;
	} bg_shift_registers_;

	// Current tile fetching state (matches hardware timing)
	struct TileFetchState {
		uint8_t fetch_cycle; // Which fetch cycle we're in (0-7)
		uint8_t current_tile_id;
		uint8_t current_attribute;
		uint8_t current_pattern_low;
		uint8_t current_pattern_high;
	} tile_fetch_state_;

	// Sprite evaluation state (for current scanline)
	struct ScanlineSprite {
		Sprite sprite_data;
		uint8_t sprite_index;	   // Original OAM sprite index (0-63, used for sprite 0 detection)
		uint8_t pattern_data_low;  // Low bit plane for current row
		uint8_t pattern_data_high; // High bit plane for current row
		bool is_sprite_0;		   // True if this is sprite 0
	};
	std::array<ScanlineSprite, 8> scanline_sprites_current_; // Sprites rendering on current scanline
	std::array<ScanlineSprite, 8> scanline_sprites_next_;	 // Sprites prepared for next scanline
	uint8_t sprite_count_current_scanline_;					 // Number of sprites rendering on current scanline
	uint8_t sprite_count_next_scanline_;					 // Number of sprites evaluated for next scanline
	bool sprite_0_on_scanline_;								 // True if sprite 0 is on current scanline (rendering)
	bool sprite_0_on_next_scanline_;						 // True if sprite 0 is on next scanline (evaluation)
	bool sprite_0_hit_detected_;							 // Prevents multiple sprite 0 hits per frame
	uint8_t sprite_0_hit_delay_;							 // Delay counter before latching sprite 0 flag

	// Hardware-accurate sprite evaluation state machine
	enum class SpriteEvalState : uint8_t {
		ReadY,		   // Reading sprite Y position from OAM
		CheckRange,	   // Checking if sprite is in range for next scanline
		CopySprite,	   // Copying sprite bytes to secondary OAM
		OverflowCheck, // Checking for 9th+ sprite (overflow detection)
		OverflowBug,   // Emulating sprite overflow hardware bug
		Done		   // Evaluation complete, waiting for cycle 256
	};
	SpriteEvalState sprite_eval_state_; // Current state in evaluation state machine
	uint8_t sprite_eval_n_;				// Current OAM sprite index being evaluated (0-63)
	uint8_t sprite_eval_m_;				// Current byte within sprite (0-3: Y, Tile, Attr, X)
	uint8_t sprite_eval_buffer_;		// Temporary buffer for Y value during range check
	uint8_t secondary_oam_index_;		// Write position in secondary OAM (0-31)
	bool sprite_overflow_detected_;		// Hardware sprite overflow flag state

	// Diagnostic tracing (auto-triggers when PPU state looks wrong)
	bool diag_trace_active_ = false;	// True when detailed logging is enabled
	int diag_trace_frames_ = 0;			// Remaining frames to trace
	uint16_t diag_last_frame_vram_ = 0; // VRAM address at start of previous frame
	int diag_stable_frames_ = 0;		// Count of consecutive frames with same coarse_y

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
	uint8_t read_oamdata(); // Simple direct OAM read with auto-increment
	uint8_t read_ppudata();

	void write_ppuctrl(uint8_t value);
	void write_ppumask(uint8_t value);
	void write_oamaddr(uint8_t value);
	void write_oamdata(uint8_t value);
	void write_ppuscroll(uint8_t value);
	void write_ppuaddr(uint8_t value);
	void write_ppudata(uint8_t value);

	// OAM operations
	void clear_secondary_oam();
	void perform_sprite_evaluation_cycle();
	void prepare_scanline_sprites(); // Convert secondary OAM to scanline sprites with pattern data
	void handle_sprite_overflow_bug();

	// Hardware timing features
	void handle_odd_frame_skip();
	void handle_vblank_timing();
	void handle_rendering_disable_mid_scanline();
	void advance_to_cycle(uint16_t target_cycle);

	// Bus conflicts and corruption
	void handle_vram_address_corruption();
	uint8_t read_open_bus();
	void update_io_bus(uint8_t value);
	void handle_palette_mirroring(uint16_t &address);

	// Fine X control and pixel selection (consolidated background pixel methods)
	void update_fine_x_scroll(uint8_t value);
	uint8_t select_pixel_from_shift_register(uint16_t shift_reg, uint8_t fine_x) const;
	uint8_t extract_background_bits_at_fine_x() const;
	uint8_t extract_attribute_bits_at_fine_x() const;
	// Note: get_background_pixel_with_fine_x functionality integrated into get_background_pixel_from_shift_registers

	// Memory access helpers
	uint8_t read_ppu_memory(uint16_t address);
	void write_ppu_memory(uint16_t address, uint8_t value);

	// Rendering helpers
	void render_pixel();
	void clear_frame_buffer();

	// Pixel multiplexer and priority resolution
	uint8_t multiplex_background_sprite_pixels(uint8_t bg_pixel, uint8_t sprite_pixel, bool sprite_priority);
	uint32_t apply_color_emphasis(uint32_t color);
	bool is_transparent_color(uint8_t palette_index);
	// Note: resolve_pixel_priority functionality merged into multiplex_background_sprite_pixels

	// Hardware-accurate register behavior
	uint8_t read_oamdata_during_rendering(); // Enhanced version that handles rendering behavior
	void handle_ppustatus_race_condition();

	// Background rendering (Phase 2)
	void render_background_pixel();
	uint8_t get_background_pixel_at_current_position();
	uint8_t fetch_nametable_byte(uint16_t nametable_addr);
	uint8_t fetch_attribute_byte(uint16_t nametable_addr);
	uint16_t fetch_pattern_data(uint8_t tile_index, uint8_t fine_y, bool background_table);
	uint8_t get_background_palette_index(uint16_t pattern_data, uint8_t attribute, uint8_t fine_x);
	uint32_t get_palette_color(uint8_t palette_index);

	// Hardware-accurate background tile fetching
	void shift_background_registers();
	void load_shift_registers();
	uint8_t get_background_pixel_from_shift_registers(); // Enhanced with fine X support
	void perform_tile_fetch_cycle();

	// Sprite rendering (Phase 3)
	uint8_t get_sprite_pixel_at_current_position(bool &sprite_priority, bool &sprite0_candidate);
	uint8_t fetch_sprite_pattern_data_raw(uint8_t tile_index, uint8_t fine_y, bool sprite_table);
	bool check_sprite_0_hit(uint8_t bg_pixel, uint8_t sprite_pixel, uint8_t x_pos); // Enhanced with edge case support
	void render_combined_pixel(uint8_t bg_pixel, uint8_t sprite_pixel, bool sprite_priority, uint8_t x_pos,
							   uint8_t y_pos);

	// MMC3 A12 tracking for IRQ counter
	void track_a12_line(uint16_t address);

	// Advanced scrolling (Phase 4)
	void increment_vram_address(); // Consolidated VRAM increment logic
	void increment_oam_address();  // Consolidated OAM address increment logic
	void set_vblank_flag();		   // Consolidated VBlank manipulation
	void clear_vblank_flag();
	void copy_horizontal_scroll();
	void copy_vertical_scroll();
	void increment_coarse_x();
	void increment_fine_y();
	void clear_shift_registers(); // Consolidated shift register clearing
	uint16_t get_current_nametable_address();
	uint16_t get_current_attribute_address();
	uint8_t get_fine_y_scroll();

	// NMI generation
	void check_nmi();

	// Internal state helpers
	bool is_rendering_enabled() const;
	bool is_background_enabled() const;
	bool is_sprites_enabled() const;
	bool is_oam_access_restricted() const; // Check if OAM access should return 0xFF
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

// OAM DMA timing
constexpr uint16_t OAM_DMA_CYCLES = 513;		// Total OAM DMA cycles (512 + 1 dummy read)
constexpr uint16_t OAM_DMA_ALIGNMENT_CYCLE = 1; // Additional cycle for odd CPU cycle alignment

// Sprite evaluation timing (cycles 65-256 of visible scanlines)
// Hardware evaluates all 64 sprites within 192 cycles using byte-by-byte state machine
// Our cycle-accurate implementation: 2 cycles/sprite minimum + copy cycles for matching sprites
// Variable timing: ReadY(1 cycle) → CheckRange(1 cycle) → CopySprite(3 cycles if in range)
constexpr uint16_t SPRITE_EVAL_START_CYCLE = 65;
constexpr uint16_t SPRITE_EVAL_END_CYCLE = 256;
constexpr uint8_t MAX_SPRITES_PER_SCANLINE = 8;

// Odd frame skip (cycle 339 skipped on odd frames during pre-render if rendering enabled)
constexpr uint16_t ODD_FRAME_SKIP_CYCLE = 339;

// Hardware quirk timing
constexpr uint8_t NMI_DELAY_CYCLES = 2;		 // NMI generation delay
constexpr uint8_t PPUSTATUS_RACE_WINDOW = 3; // VBlank race condition window
} // namespace PPUTiming

/// PPU memory access constants
namespace PPUMemoryConstants {
// Palette mirroring addresses ($3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C)
constexpr uint16_t PALETTE_MIRROR_MASK = 0x0013; // Mask for palette mirroring
constexpr uint16_t PALETTE_BASE = 0x3F00;
constexpr uint16_t PALETTE_SIZE = 0x20;

// Open bus behavior
constexpr uint16_t OPEN_BUS_DECAY_CYCLES = 600; // Approximate open bus decay time

// VRAM address bus conflicts
constexpr uint16_t VRAM_BUS_CONFLICT_MASK = 0x3FFF;
} // namespace PPUMemoryConstants

} // namespace nes
