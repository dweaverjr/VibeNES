#pragma once

#include "core/types.hpp"

namespace nes {

// Forward declaration
class NesSystem;

/// Placeholder for future memory viewer debug window
class MemoryViewer {
  public:
	// TODO: Implement when core system is ready

	/// Simple console memory dump for now
	static void print_memory_dump(const NesSystem *system, Address start_addr, std::size_t length);

	/// Print specific memory regions
	static void print_zero_page(const NesSystem *system);
	static void print_stack(const NesSystem *system);
	static void print_ppu_registers(const NesSystem *system);

  private:
	// Future implementation will include:
	// - Hex dump view of CPU and PPU memory
	// - Real-time memory change highlighting
	// - Goto address functionality
	// - ASCII view alongside hex
	// - Memory region selection (CPU, PPU, OAM, Palette)
};

} // namespace nes
