#pragma once

namespace nes {

// Forward declarations
class NesSystem;

/// Placeholder for future GUI implementation
/// Will handle SDL2/ImGui-based debugging interface
class EmulatorGUI {
  public:
	// TODO: Implement when core system is ready

	/// Simple console debug output for now
	static void print_system_state(const NesSystem *system);

	/// Simple console debug for memory operations
	static void print_memory_access(const char *operation, std::uint16_t address, std::uint8_t value);

	/// Simple console debug for CPU state
	static void print_cpu_state(const NesSystem *system);

  private:
	// Future implementation will include:
	// - SDL2 window management
	// - ImGui integration
	// - Debug windows (disassembler, memory viewer, PPU debug)
	// - Real-time emulation display
};

} // namespace nes
