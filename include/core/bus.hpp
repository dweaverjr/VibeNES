#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <memory>

namespace nes {

// Forward declarations
class Ram;
class PPUStub;
class APUStub;
class ControllerStub;
class Cartridge;

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
	void connect_ppu(std::shared_ptr<PPUStub> ppu);
	void connect_apu(std::shared_ptr<APUStub> apu);
	void connect_controllers(std::shared_ptr<ControllerStub> controllers);
	void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

	// Debug interface
	void debug_print_memory_map() const;

  private:
	// Connected components
	std::shared_ptr<Ram> ram_;
	std::shared_ptr<PPUStub> ppu_;
	std::shared_ptr<APUStub> apu_;
	std::shared_ptr<ControllerStub> controllers_;
	std::shared_ptr<Cartridge> cartridge_;

	// Address decoding helpers
	[[nodiscard]] bool is_ram_address(Address address) const noexcept;
	[[nodiscard]] bool is_ppu_address(Address address) const noexcept;
	[[nodiscard]] bool is_apu_address(Address address) const noexcept;
	[[nodiscard]] bool is_apu_read_address(Address address) const noexcept;
	[[nodiscard]] bool is_controller_address(Address address) const noexcept;
	[[nodiscard]] bool is_cartridge_address(Address address) const noexcept;

	// Test memory for high addresses (temporary solution for testing)
	// TODO: Replace with proper cartridge ROM when implemented
	mutable std::array<Byte, 0x8000> test_high_memory_{};
	mutable std::array<bool, 0x8000> test_high_memory_valid_{};

	// Open bus simulation
	mutable Byte last_bus_value_ = 0xFF;
};

} // namespace nes
