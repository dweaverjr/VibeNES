#include "ppu/ppu.hpp"
#include "cartridge/cartridge.hpp"
#include "cartridge/mappers/mapper.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "ppu/nes_palette.hpp"
#include <cstring>
#include <iostream>

namespace nes {

PPU::PPU()
	: current_cycle_(0), current_scanline_(0), frame_counter_(0), frame_ready_(false), control_register_(0),
	  mask_register_(0), status_register_(0), oam_address_(0), vram_address_(0), temp_vram_address_(0),
	  fine_x_scroll_(0), write_toggle_(false), read_buffer_(0), sprite_count_current_scanline_(0),
	  sprite_0_on_scanline_(false), bus_(nullptr), cpu_(nullptr), cartridge_(nullptr) {

	// Initialize background shift registers
	bg_shift_registers_ = {};
	tile_fetch_state_ = {};

	power_on();
}

void PPU::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
	cartridge_ = cartridge;

	// Update nametable mirroring mode based on cartridge
	if (cartridge_) {
		auto mirroring = cartridge_->get_mirroring();
		bool vertical = (mirroring == Mapper::Mirroring::Vertical);
		memory_.set_mirroring_mode(vertical);
	}
}

void PPU::power_on() {
	// PPU power-on state
	current_cycle_ = 0;
	current_scanline_ = 0;
	frame_counter_ = 0;
	frame_ready_ = false;

	// Registers power-on to 0
	control_register_ = 0;
	mask_register_ = 0;
	status_register_ = 0;
	oam_address_ = 0;

	// Internal state
	vram_address_ = 0;
	temp_vram_address_ = 0;
	fine_x_scroll_ = 0;
	write_toggle_ = false;
	read_buffer_ = 0;

	// Initialize background shift registers to prevent artifacts
	bg_shift_registers_ = {};
	tile_fetch_state_ = {};

	// Initialize memory
	memory_.power_on();

	// Clear frame buffer
	clear_frame_buffer();
}

void PPU::reset() {
	// PPU reset behavior - most state is preserved
	control_register_ = 0;
	mask_register_ = 0;
	// status_register_ is unchanged by reset
	// oam_address_ is unchanged by reset

	// Internal latches are cleared
	write_toggle_ = false;
	fine_x_scroll_ = 0;

	// Memory state is preserved
	memory_.reset();
}

void PPU::tick(CpuCycle cycles) {
	// PPU runs at 3x CPU clock speed
	// Extract the cycle count and multiply by 3 for PPU dots
	auto cpu_cycle_count = cycles.count();
	for (std::int64_t i = 0; i < cpu_cycle_count * 3; ++i) {
		tick_internal();
	}
}

void PPU::tick_internal() {
	// Process current scanline and cycle
	switch (get_current_phase()) {
	case ScanlinePhase::VISIBLE:
		process_visible_scanline();
		break;
	case ScanlinePhase::POST_RENDER:
		process_post_render_scanline();
		break;
	case ScanlinePhase::VBLANK:
		process_vblank_scanline();
		break;
	case ScanlinePhase::PRE_RENDER:
		process_pre_render_scanline();
		break;
	}

	// Advance cycle counter
	current_cycle_++;

	// Check for end of scanline
	if (current_cycle_ >= PPUTiming::CYCLES_PER_SCANLINE) {
		current_cycle_ = 0;
		current_scanline_++;

		// Check for end of frame
		if (current_scanline_ >= PPUTiming::TOTAL_SCANLINES) {
			current_scanline_ = 0;
			frame_counter_++;
			frame_ready_ = true;
		}
	}
}

uint8_t PPU::read_register(uint16_t address) {
	// Map address to PPU register
	uint16_t register_addr = PPUConstants::REGISTER_BASE + (address & PPUConstants::REGISTER_MASK);

	switch (static_cast<PPURegister>(register_addr)) {
	case PPURegister::PPUSTATUS:
		return read_ppustatus();
	case PPURegister::OAMDATA:
		return read_oamdata();
	case PPURegister::PPUDATA:
		return read_ppudata();
	default:
		// Write-only registers return stale bus data (simplified to 0)
		return 0;
	}
}

void PPU::write_register(uint16_t address, uint8_t value) {
	// Map address to PPU register
	uint16_t register_addr = PPUConstants::REGISTER_BASE + (address & PPUConstants::REGISTER_MASK);

	switch (static_cast<PPURegister>(register_addr)) {
	case PPURegister::PPUCTRL:
		write_ppuctrl(value);
		break;
	case PPURegister::PPUMASK:
		write_ppumask(value);
		break;
	case PPURegister::OAMADDR:
		write_oamaddr(value);
		break;
	case PPURegister::OAMDATA:
		write_oamdata(value);
		break;
	case PPURegister::PPUSCROLL:
		write_ppuscroll(value);
		break;
	case PPURegister::PPUADDR:
		write_ppuaddr(value);
		break;
	case PPURegister::PPUDATA:
		write_ppudata(value);
		break;
	default:
		// Invalid register writes are ignored
		break;
	}
}

// Scanline processing (Phase 4 with proper scrolling timing)
void PPU::process_visible_scanline() {
	// Hardware-accurate PPU processing for visible scanlines

	// Initialize shift registers at the start of the scanline and pre-load first tiles
	if (current_cycle_ == 0 && is_rendering_enabled()) {
		// DON'T clear shift registers - they should contain pre-loaded data from pre-render scanline
		// The NES hardware maintains shift register state between scanlines
	}

	// During the first 8 cycles, we need to fetch the first two tiles
	// to populate the shift registers before visible rendering starts
	if (current_cycle_ < 8 && is_rendering_enabled() && is_background_enabled()) {
		perform_tile_fetch_cycle();

		// Load shift registers at the end of each tile fetch during pre-fetch
		if ((current_cycle_ & 7) == 7) {
			load_shift_registers();
		}
	}

	// Sprite evaluation happens during cycles 64-256
	if (current_cycle_ == 64 && is_rendering_enabled()) {
		evaluate_sprites_for_scanline();
	}

	// Background and sprite rendering during visible cycles (0-255)
	if (current_cycle_ < PPUTiming::VISIBLE_PIXELS && is_rendering_enabled()) {
		// Shift background registers every cycle for smooth scrolling
		if (is_background_enabled()) {
			shift_background_registers();
		}

		// Render the pixel using shift registers
		render_pixel();

		// Background tile fetching and VRAM updates (every 8 cycles, continuing from pre-fetch)
		if (is_background_enabled() && current_cycle_ >= 8) {
			perform_tile_fetch_cycle();
		}

		// VRAM address increments happen at the END of each tile fetch cycle
		// This occurs at cycles 8, 16, 24, 32, etc. (after pattern data is fetched)
		if (is_background_enabled() && ((current_cycle_ & 7) == 0) && current_cycle_ > 0) {
			increment_coarse_x();

			// Load shift registers immediately after tile fetch completes
			load_shift_registers();
		}
	} // HBLANK period: Critical VRAM address updates and tile fetching for next scanline
	else if (current_cycle_ >= 256 && current_cycle_ < 341 && is_rendering_enabled()) {
		// At cycle 256: Increment fine Y (move to next row)
		if (current_cycle_ == 256) {
			increment_fine_y();
		}

		// At cycle 257: Copy horizontal scroll from temp to current
		// This resets the horizontal position for the next scanline
		if (current_cycle_ == 257) {
			copy_horizontal_scroll();
		}

		// Continue background tile fetching for next scanline (cycles 320-335)
		// These are the first two tiles that will be rendered on the next scanline
		if (current_cycle_ >= 320 && current_cycle_ < 336 && is_background_enabled()) {
			perform_tile_fetch_cycle();

			// Increment coarse X for these pre-fetch cycles too
			if ((current_cycle_ & 7) == 7) {
				increment_coarse_x();
			}
		}

		// Additional tile fetch at cycles 336-339 for odd frame timing
		// (Real hardware behavior for frame synchronization)
		if (current_cycle_ >= 336 && current_cycle_ < 340 && is_background_enabled()) {
			// Fetch nametable bytes for timing (results unused)
			if (current_cycle_ == 337 || current_cycle_ == 339) {
				uint16_t nt_addr = get_current_nametable_address();
				memory_.read_vram(nt_addr); // Dummy read for timing
			}
		}
	}
}
void PPU::process_post_render_scanline() {
	// Post-render scanline - no rendering, just idle
}

void PPU::process_vblank_scanline() {
	// Handle VBlank start
	if (current_scanline_ == PPUTiming::VBLANK_START_SCANLINE && current_cycle_ == PPUTiming::VBLANK_SET_CYCLE) {
		// Set VBlank flag
		status_register_ |= PPUConstants::PPUSTATUS_VBLANK_MASK;
		check_nmi();
	}
}

void PPU::process_pre_render_scanline() {
	// Pre-render scanline setup and VRAM address management
	if (current_cycle_ == 1) {
		// Clear VBlank and sprite overflow flags
		status_register_ &= ~(PPUConstants::PPUSTATUS_VBLANK_MASK | PPUConstants::PPUSTATUS_SPRITE0_MASK |
							  PPUConstants::PPUSTATUS_OVERFLOW_MASK);
	}

	// Copy vertical scroll from temp address to current at specific cycles
	if (is_rendering_enabled()) {
		// Initialize shift registers at start of pre-render to prepare for next frame
		if (current_cycle_ == 0) {
			bg_shift_registers_.pattern_low_shift = 0;
			bg_shift_registers_.pattern_high_shift = 0;
			bg_shift_registers_.attribute_low_shift = 0;
			bg_shift_registers_.attribute_high_shift = 0;
		}

		// Copy horizontal scroll
		if (current_cycle_ == 257) {
			copy_horizontal_scroll();
		}

		// Copy vertical scroll during cycles 280-304
		// This is CRITICAL - it copies the nametable selection from PPUCTRL!
		if (current_cycle_ >= 280 && current_cycle_ <= 304) {
			copy_vertical_scroll();
		}

		// Background tile fetching simulation during pre-render (dummy fetches)
		if (current_cycle_ < 256 && is_background_enabled()) {
			perform_tile_fetch_cycle();

			// Increment coarse X at the end of each tile fetch (cycles 8, 16, 24, etc.)
			if ((current_cycle_ & 7) == 0 && current_cycle_ > 0) {
				increment_coarse_x();
			}
		}

		// Critical: Continue fetching during HBLANK (cycles 320-335)
		// These fetches prepare the first two tiles for the next frame and must be precise
		if (current_cycle_ >= 320 && current_cycle_ < 336 && is_background_enabled()) {
			perform_tile_fetch_cycle();

			// Increment coarse X and load shift registers for the initial tiles of next frame
			if ((current_cycle_ & 7) == 7) {
				increment_coarse_x();
				load_shift_registers();
			}
		}

		if (current_cycle_ == 256) {
			increment_fine_y();
		}
	}
} // Register read handlers
uint8_t PPU::read_ppustatus() {
	uint8_t result = status_register_;

	// Clear VBlank flag after reading
	status_register_ &= ~PPUConstants::PPUSTATUS_VBLANK_MASK;

	// Reset write toggle
	write_toggle_ = false;

	return result;
}

uint8_t PPU::read_oamdata() {
	return memory_.read_oam(oam_address_);
}

uint8_t PPU::read_ppudata() {
	uint8_t result = read_buffer_;

	// Read from PPU memory
	read_buffer_ = read_ppu_memory(vram_address_);

	// Palette reads return immediately (no buffering)
	if (vram_address_ >= PPUMemoryMap::PALETTE_START) {
		result = read_buffer_;
	}

	// Increment VRAM address
	if (control_register_ & PPUConstants::PPUCTRL_INCREMENT_MASK) {
		vram_address_ += 32; // Increment by 32 (down)
	} else {
		vram_address_ += 1; // Increment by 1 (across)
	}

	return result;
}

// Register write handlers
void PPU::write_ppuctrl(uint8_t value) {
	control_register_ = value;

	// Update temporary VRAM address nametable bits
	temp_vram_address_ = (temp_vram_address_ & ~0x0C00) | ((static_cast<uint16_t>(value) & 0x03) << 10);

	check_nmi();
}

void PPU::write_ppumask(uint8_t value) {
	mask_register_ = value;
}

void PPU::write_oamaddr(uint8_t value) {
	oam_address_ = value;
}

void PPU::write_oamdata(uint8_t value) {
	memory_.write_oam(oam_address_, value);
	oam_address_++; // Auto-increment OAM address
}

void PPU::write_ppuscroll(uint8_t value) {
	if (!write_toggle_) {
		// First write: X scroll
		temp_vram_address_ = (temp_vram_address_ & ~0x001F) | ((value >> 3) & 0x1F);
		fine_x_scroll_ = value & 0x07;
		write_toggle_ = true;
	} else {
		// Second write: Y scroll
		temp_vram_address_ = (temp_vram_address_ & ~0x73E0) | (((static_cast<uint16_t>(value) & 0xF8) << 2) |
															   ((static_cast<uint16_t>(value) & 0x07) << 12));
		write_toggle_ = false;
	}
}

void PPU::write_ppuaddr(uint8_t value) {
	if (!write_toggle_) {
		// First write: high byte
		temp_vram_address_ = (temp_vram_address_ & 0x00FF) | ((static_cast<uint16_t>(value) & 0x3F) << 8);
		write_toggle_ = true;
	} else {
		// Second write: low byte
		temp_vram_address_ = (temp_vram_address_ & 0xFF00) | value;
		vram_address_ = temp_vram_address_;
		write_toggle_ = false;
	}
}

void PPU::write_ppudata(uint8_t value) {
	write_ppu_memory(vram_address_, value);

	// Increment VRAM address
	if (control_register_ & PPUConstants::PPUCTRL_INCREMENT_MASK) {
		vram_address_ += 32; // Increment by 32 (down)
	} else {
		vram_address_ += 1; // Increment by 1 (across)
	}
}

// Memory access helpers
uint8_t PPU::read_ppu_memory(uint16_t address) {
	address &= 0x3FFF; // 14-bit address space

	if (address < PPUMemoryMap::PATTERN_TABLE_1_END + 1) {
		// Pattern tables - read from cartridge
		return memory_.read_pattern_table(address);
	} else if (address < PPUMemoryMap::NAMETABLE_MIRROR_END + 1) {
		// Nametables and mirrors
		return memory_.read_vram(address);
	} else {
		// Palette RAM and mirrors
		return memory_.read_palette(address & 0x1F);
	}
}

void PPU::write_ppu_memory(uint16_t address, uint8_t value) {
	address &= 0x3FFF; // 14-bit address space

	if (address < PPUMemoryMap::PATTERN_TABLE_1_END + 1) {
		// Pattern tables - writes may go to cartridge CHR RAM (if present)
		// For now, ignore writes to pattern tables
	} else if (address < PPUMemoryMap::NAMETABLE_MIRROR_END + 1) {
		// Nametables and mirrors
		memory_.write_vram(address, value);
	} else {
		// Palette RAM and mirrors
		memory_.write_palette(address & 0x1F, value);
	}
}

// Helper functions
ScanlinePhase PPU::get_current_phase() const {
	if (current_scanline_ < PPUTiming::VISIBLE_SCANLINES) {
		return ScanlinePhase::VISIBLE;
	} else if (current_scanline_ == PPUTiming::POST_RENDER_SCANLINE) {
		return ScanlinePhase::POST_RENDER;
	} else if (current_scanline_ <= PPUTiming::VBLANK_END_SCANLINE) {
		return ScanlinePhase::VBLANK;
	} else {
		return ScanlinePhase::PRE_RENDER;
	}
}

bool PPU::is_rendering_enabled() const {
	return is_background_enabled() || is_sprites_enabled();
}

bool PPU::is_background_enabled() const {
	return (mask_register_ & 0x08) != 0; // Bit 3 of PPUMASK
}

bool PPU::is_sprites_enabled() const {
	return (mask_register_ & 0x10) != 0; // Bit 4 of PPUMASK
}

void PPU::render_pixel() {
	// Phase 3: Combined background and sprite rendering
	if (current_scanline_ < 240 && current_cycle_ < 256) {
		uint8_t bg_pixel = 0;
		uint8_t sprite_pixel = 0;
		bool sprite_priority = false;

		// Render background pixel
		if (is_background_enabled()) {
			bg_pixel = get_background_pixel_at_current_position();
		}

		// Render sprite pixel
		if (is_sprites_enabled()) {
			sprite_pixel = get_sprite_pixel_at_current_position(sprite_priority);
		}

		// Combine background and sprite pixels with priority handling
		render_combined_pixel(bg_pixel, sprite_pixel, sprite_priority, current_cycle_, current_scanline_);
	}
}

void PPU::clear_frame_buffer() {
	frame_buffer_.fill(0xFF000000); // Clear to black
}

void PPU::check_nmi() {
	// Generate NMI if VBlank is set and NMI is enabled
	if ((status_register_ & PPUConstants::PPUSTATUS_VBLANK_MASK) &&
		(control_register_ & PPUConstants::PPUCTRL_NMI_MASK) && cpu_) {
		// Signal NMI to CPU
		cpu_->trigger_nmi();
	}
}

// =============================================================================
// Background Rendering (Phase 2)
// =============================================================================

void PPU::render_background_pixel() {
	// Calculate current pixel position
	uint16_t pixel_x = current_cycle_;
	uint16_t pixel_y = current_scanline_;

	// Apply scroll offset
	uint16_t scroll_x = pixel_x + (fine_x_scroll_ & 0x07);
	uint16_t scroll_y = pixel_y;

	// Calculate nametable address
	uint16_t nametable_x = scroll_x / 8;
	uint16_t nametable_y = scroll_y / 8;
	uint16_t nametable_addr = 0x2000 + (nametable_y * 32) + nametable_x;

	// Handle nametable selection from PPUCTRL
	uint8_t nametable_select = control_register_ & PPUConstants::PPUCTRL_NAMETABLE_MASK;
	if (nametable_select & 0x01) {
		nametable_addr += 0x0400; // Select right nametable
	}
	if (nametable_select & 0x02) {
		nametable_addr += 0x0800; // Select bottom nametable
	}

	// Fetch tile data
	uint8_t tile_index = fetch_nametable_byte(nametable_addr);
	uint8_t attribute = fetch_attribute_byte(nametable_addr);

	// Calculate fine Y offset within tile
	uint8_t fine_y = scroll_y % 8;

	// Fetch pattern data
	bool background_table = (control_register_ & PPUConstants::PPUCTRL_BG_PATTERN_MASK) != 0;
	uint16_t pattern_data = fetch_pattern_data(tile_index, fine_y, background_table);

	// Calculate fine X offset within tile
	uint8_t fine_x = scroll_x % 8;

	// Get palette index for this pixel
	uint8_t palette_index = get_background_palette_index(pattern_data, attribute, fine_x);

	// Convert to color and store in frame buffer
	size_t pixel_index = pixel_y * 256 + pixel_x;
	frame_buffer_[pixel_index] = get_palette_color(palette_index);
}

uint8_t PPU::fetch_nametable_byte(uint16_t nametable_addr) {
	return memory_.read_vram(nametable_addr);
}

uint8_t PPU::fetch_attribute_byte(uint16_t nametable_addr) {
	// Calculate attribute table address
	// Each attribute byte covers a 4x4 tile area (32x32 pixels)
	uint16_t tile_x = (nametable_addr - 0x2000) % 32;
	uint16_t tile_y = (nametable_addr - 0x2000) / 32;
	uint16_t attr_x = tile_x / 4;
	uint16_t attr_y = tile_y / 4;

	// Attribute table starts at +0x3C0 from nametable base
	uint16_t nametable_base = nametable_addr & 0xFC00; // Get nametable base (0x2000, 0x2400, etc.)
	uint16_t attr_addr = nametable_base + 0x3C0 + (attr_y * 8) + attr_x;

	uint8_t attribute_byte = memory_.read_vram(attr_addr);

	// Extract 2-bit palette for this 2x2 tile group within the 4x4 area
	uint8_t sub_x = (tile_x % 4) / 2;
	uint8_t sub_y = (tile_y % 4) / 2;
	uint8_t shift = (sub_y * 2 + sub_x) * 2;

	return (attribute_byte >> shift) & 0x03;
}

uint16_t PPU::fetch_pattern_data(uint8_t tile_index, uint8_t fine_y, bool background_table) {
	// Calculate pattern table address
	uint16_t pattern_base = background_table ? 0x1000 : 0x0000;
	uint16_t pattern_addr = pattern_base + (tile_index * 16) + fine_y;

	// Each tile is 16 bytes: 8 bytes for low bit plane, 8 bytes for high bit plane
	uint8_t low_byte = 0x00;
	uint8_t high_byte = 0x00;

	// Read from cartridge CHR ROM/RAM with support for dynamic banking
	if (cartridge_) {
		// Read low bit plane
		low_byte = cartridge_->ppu_read(pattern_addr);
		// Read high bit plane
		high_byte = cartridge_->ppu_read(pattern_addr + 8);
	}

	return (static_cast<uint16_t>(high_byte) << 8) | low_byte;
}

uint8_t PPU::get_background_palette_index(uint16_t pattern_data, uint8_t attribute, uint8_t fine_x) {
	// Extract pixel from pattern data (MSB first)
	uint8_t bit_shift = 7 - fine_x;
	uint8_t low_bit = (pattern_data >> bit_shift) & 0x01;
	uint8_t high_bit = (pattern_data >> (8 + bit_shift)) & 0x01;

	// Combine to get 2-bit pixel value
	uint8_t pixel_value = (high_bit << 1) | low_bit;

	// If pixel value is 0, use backdrop color (palette index 0)
	if (pixel_value == 0) {
		return 0;
	}

	// Background palettes are at 0x00-0x0F (not 0x10+ like sprites)
	// Each palette has 4 colors, attribute selects which palette (0-3)
	return (attribute * 4) + pixel_value;
}

uint32_t PPU::get_palette_color(uint8_t palette_index) {
	// Read from palette memory
	uint8_t color_index = memory_.read_palette(palette_index);

	// Use the accurate NES color palette
	return NESPalette::get_rgba_color(color_index);
}

// =============================================================================
// Helper Functions for Refactored Rendering
// =============================================================================

// =============================================================================
// Sprite Rendering (Phase 3)
// =============================================================================

uint8_t PPU::get_sprite_pixel_at_current_position(bool &sprite_priority) {
	uint8_t current_x = current_cycle_;

	// Check for left-edge clipping first
	if (current_x < 8 && !(mask_register_ & PPUConstants::PPUMASK_SHOW_SPRITES_LEFT_MASK)) {
		sprite_priority = false;
		return 0; // Sprites clipped in leftmost 8 pixels
	}

	sprite_priority = false; // Default: sprite in front

	// Check all sprites on current scanline (front to back priority)
	for (int i = sprite_count_current_scanline_ - 1; i >= 0; i--) {
		const ScanlineSprite &sprite = scanline_sprites_[i];

		// Check if current pixel is within sprite bounds
		if (current_x >= sprite.sprite_data.x_position && current_x < sprite.sprite_data.x_position + 8) {

			// Calculate sprite-relative X position
			uint8_t sprite_x = current_x - sprite.sprite_data.x_position;

			// Handle horizontal flip
			if (sprite.sprite_data.attributes.flip_horizontal) {
				sprite_x = 7 - sprite_x;
			}

			// Get pixel from sprite pattern data
			uint8_t bit_shift = 7 - sprite_x;
			uint8_t low_bit = (sprite.pattern_data_low >> bit_shift) & 0x01;
			uint8_t high_bit = (sprite.pattern_data_high >> bit_shift) & 0x01;
			uint8_t pixel_value = (high_bit << 1) | low_bit;

			// If pixel is transparent, continue to next sprite
			if (pixel_value == 0) {
				continue;
			}

			// Get sprite priority and palette
			sprite_priority = sprite.sprite_data.attributes.priority;
			uint8_t palette_index = 0x10 + (sprite.sprite_data.attributes.palette * 4) + pixel_value;

			// Check for sprite 0 hit
			if (sprite.sprite_index == 0 && sprite_0_on_scanline_) {
				// Get background pixel for sprite 0 hit detection
				uint8_t bg_pixel = get_background_pixel_at_current_position();
				if (check_sprite_0_hit(bg_pixel, palette_index, current_x)) {
					status_register_ |= PPUConstants::PPUSTATUS_SPRITE0_MASK;
				}
			}

			return palette_index;
		}
	}

	return 0; // No sprite pixel (transparent)
}

void PPU::evaluate_sprites_for_scanline() {
	sprite_count_current_scanline_ = 0;
	sprite_0_on_scanline_ = false;

	uint8_t scanline = current_scanline_;

	// Check sprite size (8x8 or 8x16)
	uint8_t sprite_height = (control_register_ & PPUConstants::PPUCTRL_SPRITE_SIZE_MASK) ? 16 : 8;

	// Get OAM data
	const auto &oam_data = memory_.get_oam();

	// Evaluate all 64 sprites
	for (uint8_t sprite_index = 0; sprite_index < 64; sprite_index++) {
		// If we already have 8 sprites, set overflow flag and stop
		if (sprite_count_current_scanline_ >= 8) {
			status_register_ |= PPUConstants::PPUSTATUS_OVERFLOW_MASK;
			break;
		}

		// Get sprite data (4 bytes per sprite)
		uint8_t base_addr = sprite_index * 4;
		Sprite sprite;
		sprite.y_position = oam_data[base_addr];
		sprite.tile_index = oam_data[base_addr + 1];
		sprite.attributes = *reinterpret_cast<const SpriteAttributes *>(&oam_data[base_addr + 2]);
		sprite.x_position = oam_data[base_addr + 3];

		// Check if sprite is on current scanline
		if (scanline >= sprite.y_position && scanline < sprite.y_position + sprite_height) {
			// Calculate sprite Y offset
			uint8_t sprite_y = scanline - sprite.y_position;

			// Handle vertical flip
			if (sprite.attributes.flip_vertical) {
				sprite_y = (sprite_height - 1) - sprite_y;
			}

			// Fetch sprite pattern data
			bool sprite_table = (control_register_ & PPUConstants::PPUCTRL_SPRITE_PATTERN_MASK) != 0;

			ScanlineSprite &scanline_sprite = scanline_sprites_[sprite_count_current_scanline_];
			scanline_sprite.sprite_data = sprite;
			scanline_sprite.sprite_index = sprite_index;

			// For 8x16 sprites, handle pattern table selection differently
			if (sprite_height == 16) {
				// In 8x16 mode, bit 0 of tile_index selects pattern table
				sprite_table = (sprite.tile_index & 0x01) != 0;
				uint8_t tile_number = sprite.tile_index & 0xFE; // Clear LSB

				if (sprite_y < 8) {
					// Top half
					scanline_sprite.pattern_data_low =
						fetch_sprite_pattern_data_raw(tile_number, sprite_y, sprite_table);
					scanline_sprite.pattern_data_high =
						fetch_sprite_pattern_data_raw(tile_number, sprite_y + 8, sprite_table);
				} else {
					// Bottom half
					scanline_sprite.pattern_data_low =
						fetch_sprite_pattern_data_raw(tile_number + 1, sprite_y - 8, sprite_table);
					scanline_sprite.pattern_data_high =
						fetch_sprite_pattern_data_raw(tile_number + 1, sprite_y, sprite_table);
				}
			} else {
				// 8x8 sprites
				scanline_sprite.pattern_data_low =
					fetch_sprite_pattern_data_raw(sprite.tile_index, sprite_y, sprite_table);
				scanline_sprite.pattern_data_high =
					fetch_sprite_pattern_data_raw(sprite.tile_index, sprite_y + 8, sprite_table);
			}

			// Track sprite 0
			if (sprite_index == 0) {
				sprite_0_on_scanline_ = true;
			}

			sprite_count_current_scanline_++;
		}
	}
}

uint8_t PPU::fetch_sprite_pattern_data_raw(uint8_t tile_index, uint8_t fine_y, bool sprite_table) {
	// Calculate pattern table address
	uint16_t pattern_base = sprite_table ? 0x1000 : 0x0000;
	uint16_t pattern_addr = pattern_base + (tile_index * 16) + fine_y;

	// Read from cartridge CHR ROM/RAM with support for dynamic banking
	// This allows mappers to switch CHR banks during sprite evaluation
	if (cartridge_) {
		return cartridge_->ppu_read(pattern_addr);
	}

	return 0x00;
}

bool PPU::check_sprite_0_hit(uint8_t bg_pixel, uint8_t sprite_pixel, uint8_t x_pos) {
	// Sprite 0 hit conditions:
	// 1. Both background and sprite pixel must be non-transparent
	// 2. X position must be > 0 (sprite 0 hit doesn't occur at x=0)
	// 3. Both background and sprites must be enabled
	// 4. Not in leftmost 8 pixels if clipping is enabled

	if (bg_pixel == 0 || sprite_pixel == 0) {
		return false; // Either pixel is transparent
	}

	if (x_pos == 0) {
		return false; // No hit at x=0
	}

	if (!is_background_enabled() || !is_sprites_enabled()) {
		return false; // Rendering disabled
	}

	// Check clipping
	if (x_pos < 8) {
		if (!(mask_register_ & PPUConstants::PPUMASK_SHOW_BG_LEFT_MASK) ||
			!(mask_register_ & PPUConstants::PPUMASK_SHOW_SPRITES_LEFT_MASK)) {
			return false; // Clipping enabled and in clipped region
		}
	}

	return true;
}

void PPU::render_combined_pixel(uint8_t bg_pixel, uint8_t sprite_pixel, bool sprite_priority, uint8_t x_pos,
								uint8_t y_pos) {
	uint8_t final_pixel;

	// Priority decision:
	// 1. If sprite pixel is transparent, use background
	// 2. If background pixel is transparent, use sprite
	// 3. If both opaque, use priority bit

	if (sprite_pixel == 0) {
		// No sprite pixel, use background
		final_pixel = bg_pixel;
	} else if (bg_pixel == 0) {
		// No background pixel, use sprite
		final_pixel = sprite_pixel;
	} else {
		// Both pixels present, check priority
		if (sprite_priority) {
			// Sprite behind background
			final_pixel = bg_pixel;
		} else {
			// Sprite in front of background
			final_pixel = sprite_pixel;
		}
	}

	// If no pixel at all, use backdrop color
	if (final_pixel == 0) {
		final_pixel = 0; // Backdrop color (palette index 0)
	}

	// Render to frame buffer
	size_t pixel_index = y_pos * 256 + x_pos;
	frame_buffer_[pixel_index] = get_palette_color(final_pixel);
}

// =============================================================================
// Advanced Scrolling System (Phase 4)
// =============================================================================

/*
 * NES PPU VRAM Address Register Layout (15 bits):
 * yyy NN YYYYY XXXXX
 * ||| || ||||| +++++-- coarse X scroll
 * ||| || +++++-------- coarse Y scroll
 * ||| ++-------------- nametable select
 * +++----------------- fine Y scroll
 */

void PPU::update_vram_address_x() {
	// Called at end of each visible scanline
	if (is_rendering_enabled()) {
		copy_horizontal_scroll();
	}
}

void PPU::update_vram_address_y() {
	// Called at end of pre-render scanline
	if (is_rendering_enabled()) {
		copy_vertical_scroll();
	}
}

void PPU::copy_horizontal_scroll() {
	// Copy horizontal scroll bits from temp to current VRAM address
	// Copies: coarse X (bits 0-4) and horizontal nametable (bit 10)

	vram_address_ = (vram_address_ & 0xFBE0) | (temp_vram_address_ & 0x041F);
}

void PPU::copy_vertical_scroll() {
	// Copy vertical scroll bits from temp to current VRAM address
	// Copies: coarse Y (bits 5-9), vertical nametable (bit 11), fine Y (bits 12-14)
	vram_address_ = (vram_address_ & 0x041F) | (temp_vram_address_ & 0xFBE0);
}

void PPU::increment_coarse_x() {
	// Increment coarse X and handle nametable wrapping
	if ((vram_address_ & 0x001F) == 31) {
		vram_address_ &= ~0x001F; // Reset coarse X to 0
		vram_address_ ^= 0x0400;  // Switch horizontal nametable
	} else {
		vram_address_++; // Just increment coarse X
	}
}

void PPU::increment_fine_y() {
	// Increment fine Y and handle coarse Y/nametable wrapping
	if ((vram_address_ & 0x7000) != 0x7000) {
		vram_address_ += 0x1000; // Increment fine Y
	} else {
		vram_address_ &= ~0x7000; // Reset fine Y to 0
		int coarse_y = (vram_address_ & 0x03E0) >> 5;

		if (coarse_y == 29) {
			coarse_y = 0;
			vram_address_ ^= 0x0800; // Switch vertical nametable
		} else if (coarse_y == 31) {
			coarse_y = 0; // Reset without switching nametable
		} else {
			coarse_y++;
		}

		vram_address_ = (vram_address_ & ~0x03E0) | (coarse_y << 5);
	}
}

uint16_t PPU::get_current_nametable_address() {
	// Extract nametable address from current VRAM address
	// Use only coarse X, coarse Y, and nametable select (NOT fine Y)
	// Fine Y is used for pattern table row selection, not nametable addressing
	uint16_t addr = 0x2000 | (vram_address_ & 0x0FFF & ~0x7000);

	// Ensure address is within nametable range
	if (addr > 0x2FFF) {
		addr = 0x2000 + (addr & 0x3FF); // Wrap to valid nametable
	}

	return addr;
}

uint16_t PPU::get_current_attribute_address() {
	// Calculate attribute table address from current VRAM address
	// Attribute tables are at +0x3C0 from each nametable base
	uint16_t base_addr = 0x23C0 | (vram_address_ & 0x0C00); // Preserve nametable selection
	uint16_t attr_offset = ((vram_address_ >> 4) & 0x38) | ((vram_address_ >> 2) & 0x07);

	uint16_t addr = base_addr | attr_offset;

	// Ensure attribute address is within valid range
	if ((addr & 0x3FF) >= 0x3C0) {
		return addr;
	} else {
		// Force to attribute table if calculation went wrong
		return base_addr;
	}
}

uint8_t PPU::get_fine_y_scroll() {
	// Extract fine Y scroll from VRAM address (bits 12-14)
	return (vram_address_ & 0x7000) >> 12;
}

// =============================================================================
// Updated Background Rendering with Proper Scrolling (Phase 4)
// =============================================================================

uint8_t PPU::get_background_pixel_at_current_position() {
	// Check for left-edge clipping first
	if (current_cycle_ < 8 && !(mask_register_ & PPUConstants::PPUMASK_SHOW_BG_LEFT_MASK)) {
		return 0; // Background clipped in leftmost 8 pixels
	}

	// Use hardware-accurate shift registers for pixel output
	return get_background_pixel_from_shift_registers();
}

uint8_t PPU::read_chr_rom(uint16_t address) const {
	if (cartridge_) {
		// Pattern table addresses are 0x0000-0x1FFF
		return cartridge_->ppu_read(address & 0x1FFF);
	}
	return 0x00; // No cartridge connected
}

// =============================================================================
// Hardware-Accurate Background Tile Fetching and Shift Registers
// =============================================================================

void PPU::shift_background_registers() {
	// Shift all background registers left by 1 bit (like real hardware)
	bg_shift_registers_.pattern_low_shift <<= 1;
	bg_shift_registers_.pattern_high_shift <<= 1;
	bg_shift_registers_.attribute_low_shift <<= 1;
	bg_shift_registers_.attribute_high_shift <<= 1;
}

void PPU::load_shift_registers() {
	// Load the next tile data into the shift registers (every 8 cycles)
	// This gives us the 2-tile lookahead that real hardware has

	// Real NES hardware: New tile data is loaded into the LOW 8 bits
	// Every pixel cycle, shift registers shift LEFT, and we read from bit 15
	// This creates the proper 2-tile pipeline: current tile shifts out high bits,
	// next tile shifts up from low bits
	bg_shift_registers_.pattern_low_shift =
		(bg_shift_registers_.pattern_low_shift & 0xFF00) | bg_shift_registers_.next_tile_pattern_low;
	bg_shift_registers_.pattern_high_shift =
		(bg_shift_registers_.pattern_high_shift & 0xFF00) | bg_shift_registers_.next_tile_pattern_high;

	// Attribute bits are expanded to 8 bits for the tile and loaded into low 8 bits
	uint8_t attr_bits = bg_shift_registers_.next_tile_attribute & 0x03;
	uint8_t attr_low = (attr_bits & 0x01) ? 0xFF : 0x00;
	uint8_t attr_high = (attr_bits & 0x02) ? 0xFF : 0x00;

	bg_shift_registers_.attribute_low_shift = (bg_shift_registers_.attribute_low_shift & 0xFF00) | attr_low;
	bg_shift_registers_.attribute_high_shift = (bg_shift_registers_.attribute_high_shift & 0xFF00) | attr_high;
}

uint8_t PPU::get_background_pixel_from_shift_registers() {
	// Extract pixel from shift registers using fine X scroll
	// The shift registers are 16-bit, with the current tile in the high 8 bits
	// and the next tile in the low 8 bits. Fine X scroll (0-7) selects which
	// bit within the current tile to use.

	uint8_t shift_amount = 15 - fine_x_scroll_;

	uint8_t pattern_low = (bg_shift_registers_.pattern_low_shift >> shift_amount) & 0x01;
	uint8_t pattern_high = (bg_shift_registers_.pattern_high_shift >> shift_amount) & 0x01;
	uint8_t attr_low = (bg_shift_registers_.attribute_low_shift >> shift_amount) & 0x01;
	uint8_t attr_high = (bg_shift_registers_.attribute_high_shift >> shift_amount) & 0x01;

	// Combine pattern bits for pixel value
	uint8_t pixel_value = (pattern_high << 1) | pattern_low;

	// If pixel is transparent, return backdrop color
	if (pixel_value == 0) {
		return 0;
	}

	// Combine attribute bits for palette selection
	uint8_t palette = (attr_high << 1) | attr_low;

	// Background palettes are at 0x00-0x0F
	return (palette * 4) + pixel_value;
}

void PPU::perform_tile_fetch_cycle() {
	// Perform one cycle of the 8-cycle tile fetch sequence
	uint8_t cycle_in_tile = current_cycle_ & 0x07;

	switch (cycle_in_tile) {
	case 1: // Fetch nametable byte
	{
		uint16_t nt_addr = get_current_nametable_address();
		tile_fetch_state_.current_tile_id = memory_.read_vram(nt_addr);
	} break;

	case 3: // Fetch attribute byte
	{
		uint16_t attr_addr = get_current_attribute_address();
		uint8_t attr_byte = memory_.read_vram(attr_addr);

		// Extract 2-bit palette for current tile position
		uint8_t coarse_x = vram_address_ & 0x1F;
		uint8_t coarse_y = (vram_address_ >> 5) & 0x1F;

		// Calculate which 2x2 group within the 4x4 attribute area
		uint8_t sub_x = (coarse_x & 2) >> 1; // 0 or 1
		uint8_t sub_y = (coarse_y & 2) >> 1; // 0 or 1
		uint8_t shift = (sub_y * 2 + sub_x) * 2;

		tile_fetch_state_.current_attribute = (attr_byte >> shift) & 0x03;
	} break;

	case 5: // Fetch pattern table low byte
	{
		bool bg_table = (control_register_ & PPUConstants::PPUCTRL_BG_PATTERN_MASK) != 0;
		uint16_t pattern_base = bg_table ? 0x1000 : 0x0000;
		uint8_t fine_y = get_fine_y_scroll();
		uint16_t pattern_addr = pattern_base + (tile_fetch_state_.current_tile_id * 16) + fine_y;

		// Support for mid-frame CHR bank switching
		// Each CHR read goes through the cartridge, allowing mappers to handle banking
		tile_fetch_state_.current_pattern_low = cartridge_ ? cartridge_->ppu_read(pattern_addr) : 0;
	} break;

	case 7: // Fetch pattern table high byte and store tile data
	{
		bool bg_table = (control_register_ & PPUConstants::PPUCTRL_BG_PATTERN_MASK) != 0;
		uint16_t pattern_base = bg_table ? 0x1000 : 0x0000;
		uint8_t fine_y = get_fine_y_scroll();
		uint16_t pattern_addr = pattern_base + (tile_fetch_state_.current_tile_id * 16) + fine_y + 8;

		// Support for mid-frame CHR bank switching
		// Each CHR read goes through the cartridge, allowing mappers to handle banking
		tile_fetch_state_.current_pattern_high = cartridge_ ? cartridge_->ppu_read(pattern_addr) : 0;

		// Store the fetched data into the "next" latches
		// Shift registers will be loaded at cycle end (0, 8, 16, etc.)
		bg_shift_registers_.next_tile_id = tile_fetch_state_.current_tile_id;
		bg_shift_registers_.next_tile_attribute = tile_fetch_state_.current_attribute;
		bg_shift_registers_.next_tile_pattern_low = tile_fetch_state_.current_pattern_low;
		bg_shift_registers_.next_tile_pattern_high = tile_fetch_state_.current_pattern_high;
	} break;

	default:
		// Other cycles do nothing for background fetching
		break;
	}
}

} // namespace nes
