#pragma once

#include "core/types.hpp"

namespace nes {

/// PPU Memory-mapped register addresses
enum class PPURegister : uint16_t {
	PPUCTRL = 0x2000,	// Write-only: PPU Control Register
	PPUMASK = 0x2001,	// Write-only: PPU Mask Register
	PPUSTATUS = 0x2002, // Read-only: PPU Status Register
	OAMADDR = 0x2003,	// Write-only: OAM Address Register
	OAMDATA = 0x2004,	// Read/Write: OAM Data Register
	PPUSCROLL = 0x2005, // Write-only: PPU Scroll Register (written twice)
	PPUADDR = 0x2006,	// Write-only: PPU Address Register (written twice)
	PPUDATA = 0x2007,	// Read/Write: PPU Data Register
};

/// PPUCTRL ($2000) - PPU Control Register bit fields
struct PPUCtrl {
	uint8_t nametable_x : 1;		// Bit 0: Base nametable address X (0: $2000, 1: $2400)
	uint8_t nametable_y : 1;		// Bit 1: Base nametable address Y (0: $2000, 1: $2800)
	uint8_t increment_mode : 1;		// Bit 2: VRAM address increment (0: +1 across, 1: +32 down)
	uint8_t sprite_pattern : 1;		// Bit 3: Sprite pattern table (0: $0000, 1: $1000)
	uint8_t background_pattern : 1; // Bit 4: Background pattern table (0: $0000, 1: $1000)
	uint8_t sprite_size : 1;		// Bit 5: Sprite size (0: 8x8, 1: 8x16)
	uint8_t master_slave : 1;		// Bit 6: PPU master/slave (unused in NES)
	uint8_t nmi_enable : 1;			// Bit 7: Generate NMI at start of VBlank (0: off, 1: on)
};

/// PPUMASK ($2001) - PPU Mask Register bit fields
struct PPUMask {
	uint8_t greyscale : 1;			  // Bit 0: Greyscale (0: color, 1: greyscale)
	uint8_t show_background_left : 1; // Bit 1: Show background in leftmost 8 pixels
	uint8_t show_sprites_left : 1;	  // Bit 2: Show sprites in leftmost 8 pixels
	uint8_t show_background : 1;	  // Bit 3: Show background
	uint8_t show_sprites : 1;		  // Bit 4: Show sprites
	uint8_t emphasize_red : 1;		  // Bit 5: Emphasize red
	uint8_t emphasize_green : 1;	  // Bit 6: Emphasize green
	uint8_t emphasize_blue : 1;		  // Bit 7: Emphasize blue
};

/// PPUSTATUS ($2002) - PPU Status Register bit fields
struct PPUStatus {
	uint8_t unused : 5;			 // Bits 0-4: Not used (returns stale PPU bus contents)
	uint8_t sprite_overflow : 1; // Bit 5: Sprite overflow (set when >8 sprites on scanline)
	uint8_t sprite_0_hit : 1;	 // Bit 6: Sprite 0 hit (set when sprite 0 overlaps background)
	uint8_t vblank : 1;			 // Bit 7: VBlank started (set at dot 1 of scanline 241)
};

/// OAM Sprite attribute byte bit fields
struct SpriteAttributes {
	uint8_t palette : 2;		 // Bits 0-1: Palette (4 to 7 of sprite palette)
	uint8_t unused : 3;			 // Bits 2-4: Unimplemented
	uint8_t priority : 1;		 // Bit 5: Priority (0: in front of background, 1: behind background)
	uint8_t flip_horizontal : 1; // Bit 6: Flip sprite horizontally
	uint8_t flip_vertical : 1;	 // Bit 7: Flip sprite vertically
};

/// OAM Sprite entry (4 bytes per sprite, 64 sprites total)
struct Sprite {
	uint8_t y_position;			 // Y position minus 1
	uint8_t tile_index;			 // Tile number from pattern table
	SpriteAttributes attributes; // Palette, flip flags, priority
	uint8_t x_position;			 // X position
};

// Register access masks and constants
namespace PPUConstants {
constexpr uint16_t REGISTER_MASK = 0x0007; // PPU registers repeat every 8 bytes
constexpr uint16_t REGISTER_BASE = 0x2000; // Base address of PPU registers
constexpr uint16_t REGISTER_END = 0x3FFF;  // End of PPU register mirror range

constexpr uint8_t PPUCTRL_NAMETABLE_MASK = 0x03;	  // Bits 0-1 for nametable selection
constexpr uint8_t PPUCTRL_INCREMENT_MASK = 0x04;	  // Bit 2 for VRAM increment mode
constexpr uint8_t PPUCTRL_SPRITE_PATTERN_MASK = 0x08; // Bit 3 for sprite pattern table
constexpr uint8_t PPUCTRL_BG_PATTERN_MASK = 0x10;	  // Bit 4 for background pattern table
constexpr uint8_t PPUCTRL_SPRITE_SIZE_MASK = 0x20;	  // Bit 5 for sprite size
constexpr uint8_t PPUCTRL_NMI_MASK = 0x80;			  // Bit 7 for NMI enable

constexpr uint8_t PPUMASK_SHOW_BG_LEFT_MASK = 0x02;		 // Bit 1: Show background in leftmost 8 pixels
constexpr uint8_t PPUMASK_SHOW_SPRITES_LEFT_MASK = 0x04; // Bit 2: Show sprites in leftmost 8 pixels
constexpr uint8_t PPUMASK_SHOW_BG_MASK = 0x08;			 // Bit 3: Show background
constexpr uint8_t PPUMASK_SHOW_SPRITES_MASK = 0x10;		 // Bit 4: Show sprites

constexpr uint8_t PPUSTATUS_VBLANK_MASK = 0x80;	  // Bit 7 for VBlank flag
constexpr uint8_t PPUSTATUS_SPRITE0_MASK = 0x40;  // Bit 6 for Sprite 0 hit
constexpr uint8_t PPUSTATUS_OVERFLOW_MASK = 0x20; // Bit 5 for sprite overflow
} // namespace PPUConstants

} // namespace nes
