#pragma once

#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "test_chr_data.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nes::test {

/// Deterministic harness that wires up a standalone PPU with a synthetic cartridge
/// and records detailed per-dot debug information for trace-driven investigations.
class PPUTraceHarness {
  public:
	struct TraceSample {
		std::uint64_t sample_index{};	 ///< Sequential capture index
		std::uint64_t frame{};			 ///< Frame counter at time of capture
		PPU::DebugState ppu_state{};	 ///< Snapshot of internal PPU registers/latches
		std::uint8_t status_register{};	 ///< PPUSTATUS ($2002)
		std::uint8_t mask_register{};	 ///< PPUMASK   ($2001)
		std::uint8_t control_register{}; ///< PPUCTRL   ($2000)
		bool sprite_0_hit{};			 ///< Convenience flag extracted from status_register
		bool sprite_overflow{};			 ///< Convenience flag extracted from status_register
		bool frame_ready{};				 ///< Whether the PPU has latched a completed frame buffer
	};

	PPUTraceHarness();

	/// Re-initialize all components to their power-on state and clear previous trace samples.
	void reset();

	/// Remove any captured samples without disturbing hardware state.
	void clear_trace();

	/// Access to underlying trace buffer.
	[[nodiscard]] const std::vector<TraceSample> &trace() const noexcept {
		return trace_;
	}

	/// Access the most recently captured sample. Undefined if trace is empty.
	[[nodiscard]] const TraceSample &latest_sample() const {
		return trace_.back();
	}

	/// Convenience accessors for connected components.
	[[nodiscard]] std::shared_ptr<PPU> ppu() const noexcept {
		return ppu_;
	}
	[[nodiscard]] std::shared_ptr<SystemBus> bus() const noexcept {
		return bus_;
	}
	[[nodiscard]] std::shared_ptr<Cartridge> cartridge() const noexcept {
		return cartridge_;
	}

	[[nodiscard]] bool is_cartridge_loaded() const noexcept;

	// ------------------------------------------------------------
	// Register / VRAM helpers mirroring existing test fixtures
	// ------------------------------------------------------------
	void write_ppu_register(std::uint16_t address, std::uint8_t value);
	std::uint8_t read_ppu_register(std::uint16_t address);

	void set_vram_address(std::uint16_t address);
	void write_vram(std::uint16_t address, std::uint8_t value);
	std::uint8_t read_vram(std::uint16_t address);
	void write_palette(std::uint16_t address, std::uint8_t value);
	void set_scroll(std::uint8_t x, std::uint8_t y);

	// ------------------------------------------------------------
	// Trace capture controls
	// ------------------------------------------------------------
	/// Advance the PPU by the specified number of dots without recording samples.
	void run_dots(std::size_t dots);

	/// Advance and capture debug samples for each dot.
	void capture_dots(std::size_t dots);

	/// Advance until the PPU reaches the requested (scanline, cycle) pair.
	/// When capture=true, every intermediate dot is captured.
	void advance_to_position(std::uint16_t target_scanline, std::uint16_t target_cycle, bool capture = false,
							 std::size_t safety_guard = 1'000'000);

	/// Advance until the PPU begins the next frame (frame counter increments).
	void advance_to_next_frame(bool capture = false, std::size_t safety_guard = 1'000'000);

	/// Capture samples while predicate returns true. Predicate receives the freshly captured sample.
	template <typename Predicate> void capture_while(Predicate predicate, std::size_t safety_guard = 1'000'000) {
		std::size_t iterations = 0;
		while (iterations < safety_guard) {
			tick_internal(true);
			++iterations;

			if (!predicate(latest_sample())) {
				return;
			}
		}
		throw std::runtime_error("capture_while exceeded safety guard");
	}

	/// Emit a human-readable multi-line dump of captured samples.
	void dump_trace(std::ostream &os, std::size_t max_samples = std::numeric_limits<std::size_t>::max()) const;

	/// Format a single trace sample as a compact string (useful for logging).
	[[nodiscard]] std::string format_sample(const TraceSample &sample) const;

  private:
	void connect_components();
	void record_sample();
	void tick_internal(bool capture);

	std::shared_ptr<SystemBus> bus_;
	std::shared_ptr<Ram> ram_;
	std::shared_ptr<Cartridge> cartridge_;
	std::shared_ptr<APU> apu_;
	std::shared_ptr<CPU6502> cpu_;
	std::shared_ptr<PPU> ppu_;

	std::uint64_t sample_counter_ = 0;
	std::vector<TraceSample> trace_;
};

} // namespace nes::test
