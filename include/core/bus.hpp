#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <memory>

namespace nes {

// Forward declarations
class Ram;

/// System Bus - Central memory and I/O interconnect
/// Handles address decoding and routes memory accesses to appropriate components
class SystemBus final : public Component {
  public:
	SystemBus();
	~SystemBus() = default;

	// Component interface
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	[[nodiscard]] const char *get_name() const noexcept override;

	// Memory interface
	[[nodiscard]] Byte read(Address address) const;
	void write(Address address, Byte value);

	// Component management
	void connect_ram(std::shared_ptr<Ram> ram);
	// TODO: Add other component connections as we implement them
	// void connect_ppu(std::shared_ptr<PPU> ppu);
	// void connect_apu(std::shared_ptr<APU> apu);

	// Debug interface
	void debug_print_memory_map() const;

  private:
	// Connected components
	std::shared_ptr<Ram> ram_;
	// std::shared_ptr<PPU> ppu_;  // TODO: Add when implemented
	// std::shared_ptr<APU> apu_;  // TODO: Add when implemented

	// Address decoding helpers
	[[nodiscard]] bool is_ram_address(Address address) const noexcept;
	[[nodiscard]] bool is_ppu_address(Address address) const noexcept;
	[[nodiscard]] bool is_apu_address(Address address) const noexcept;

	// Test memory for high addresses (temporary solution for testing)
	// TODO: Replace with proper cartridge ROM when implemented
	mutable std::array<Byte, 0x8000> test_high_memory_{};
	mutable std::array<bool, 0x8000> test_high_memory_valid_{};

	// Open bus simulation
	mutable Byte last_bus_value_ = 0xFF;
};

} // namespace nes
