#pragma once

#include "core/types.hpp"
#include <memory>

// Forward declarations
namespace nes {
class CPU6502;
class PPU;
class SystemBus;
} // namespace nes

namespace nes::gui {

/**
 * Panel for displaying timing information for CPU and PPU
 * Shows cycle counts, frequencies, and synchronization status
 */
class TimingPanel {
  public:
	TimingPanel();
	~TimingPanel() = default;

	// Non-copyable, non-movable
	TimingPanel(const TimingPanel &) = delete;
	TimingPanel &operator=(const TimingPanel &) = delete;
	TimingPanel(TimingPanel &&) = delete;
	TimingPanel &operator=(TimingPanel &&) = delete;

	/**
	 * Render the timing panel
	 * @param cpu Pointer to CPU instance (can be nullptr)
	 * @param ppu Pointer to PPU instance (can be nullptr)
	 * @param bus Pointer to system bus (can be nullptr)
	 */
	void render(nes::CPU6502 *cpu, nes::PPU *ppu, nes::SystemBus *bus);

  private:
	// Timing tracking
	std::uint64_t last_cpu_cycles_;
	std::uint64_t last_ppu_cycles_;
	std::uint64_t frame_count_;

	// Performance tracking
	float cpu_frequency_hz_;
	float ppu_frequency_hz_;
	float frame_rate_fps_;

	// Helper methods
	void render_cpu_timing(nes::CPU6502 *cpu);
	void render_ppu_timing(nes::PPU *ppu);
	void render_synchronization_info(nes::CPU6502 *cpu, nes::PPU *ppu);
	void render_performance_metrics();

	// Format helpers
	std::string format_cycles(std::uint64_t cycles) const;
	std::string format_frequency(float frequency) const;
};

} // namespace nes::gui
