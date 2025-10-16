#pragma once

#include "audio/audio_backend.hpp"
#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <memory>

namespace nes {

// Forward declarations
class Ram;
class PPU;
class APU;
class Controller;
class Cartridge;
class CPU6502;

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

	// Non-intrusive memory peek (no side effects) for debugging
	[[nodiscard]] Byte peek(Address address) const;

	// Component management
	void connect_ram(std::shared_ptr<Ram> ram);
	void connect_ppu(std::shared_ptr<PPU> ppu);
	void connect_apu(std::shared_ptr<APU> apu);
	void connect_controllers(std::shared_ptr<Controller> controllers);
	void connect_cartridge(std::shared_ptr<Cartridge> cartridge);
	void connect_cpu(std::shared_ptr<CPU6502> cpu);

	// Debug interface
	void debug_print_memory_map() const;

	// DMA interface
	[[nodiscard]] bool is_dma_active() const noexcept;

	// Audio control
	bool initialize_audio(int sample_rate = 44100, int buffer_size = 1024);
	void start_audio();
	void stop_audio();
	void set_audio_volume(float volume);
	[[nodiscard]] float get_audio_volume() const;
	[[nodiscard]] bool is_audio_playing() const;

  private:
	// Connected components
	std::shared_ptr<Ram> ram_;
	std::shared_ptr<PPU> ppu_;
	std::shared_ptr<APU> apu_;
	std::shared_ptr<Controller> controllers_;
	std::shared_ptr<Cartridge> cartridge_;
	std::shared_ptr<CPU6502> cpu_;

	// Audio backend
	std::unique_ptr<AudioBackend> audio_backend_;

	// Address decoding helpers
	[[nodiscard]] bool is_ram_address(Address address) const noexcept;
	[[nodiscard]] bool is_ppu_address(Address address) const noexcept;
	[[nodiscard]] bool is_apu_address(Address address) const noexcept;
	[[nodiscard]] bool is_apu_read_address(Address address) const noexcept;
	[[nodiscard]] bool is_controller_address(Address address) const noexcept;
	[[nodiscard]] bool is_cartridge_address(Address address) const noexcept;

	// DMA implementation
	void perform_oam_dma(Byte page);

	// Test memory for high addresses (temporary solution for testing)
	// TODO: Replace with proper cartridge ROM when implemented
	mutable std::array<Byte, 0x8000> test_high_memory_{};
	mutable std::array<bool, 0x8000> test_high_memory_valid_{};

	// Open bus simulation
	mutable Byte last_bus_value_ = 0xFF;
};

} // namespace nes
