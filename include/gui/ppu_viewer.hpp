#pragma once

#include "core/types.hpp"

namespace nes {

// Forward declaration
class NesSystem;

/// Placeholder for future PPU debug viewer
class PpuViewer {
  public:
	// TODO: Implement when core system is ready

	/// Simple console PPU state dump for now
	static void print_ppu_state(const NesSystem *system);

	/// Print pattern table info
	static void print_pattern_tables(const NesSystem *system);

	/// Print palette info
	static void print_palettes(const NesSystem *system);

	/// Print sprite info
	static void print_sprites(const NesSystem *system);

  private:
	// Future implementation will include:
	// - Pattern table visualization (CHR data)
	// - Name table display (background map)
	// - Palette viewer with colors
	// - Sprite list with properties
	// - PPU register state
	// - Real-time updates during emulation
};

} // namespace nes
