#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <iomanip>
#include <iostream>

namespace nes {

/// 2KB Work RAM with mirroring
/// NES has 2KB of RAM at $0000-$07FF, mirrored up to $2000
class Ram final : public Component {
  public:
	Ram() = default;

	// Component interface
	void tick(CpuCycle cycles) override {
		// RAM doesn't need to do anything on tick
		// (No timing-sensitive behavior)
		(void)cycles; // Suppress unused parameter warning
	}

	void reset() override {
		// RAM contents are typically undefined on reset
		// Some emulators fill with specific patterns for testing
		memory_.fill(0x00);
	}

	void power_on() override {
		// Simulate realistic random power-on state
		// Real NES RAM contains garbage values on power-up
		for (std::size_t i = 0; i < memory_.size(); ++i) {
			// Generate pseudo-random garbage pattern
			// Using simple LCG for deterministic but varied results
			memory_[i] = static_cast<Byte>((i * 17 + 42) ^ (i >> 3) ^ 0xAA);
		}
	}

	[[nodiscard]] const char *get_name() const noexcept override {
		return "Work RAM";
	}

	/// Read a byte from RAM
	/// Handles mirroring automatically
	[[nodiscard]] Byte read(Address address) const noexcept {
		const Address mirrored_addr = mirror_ram_address(address);

		if (mirrored_addr >= RAM_SIZE) {
			// Invalid RAM access - should not happen with proper mirroring
			return 0xFF; // Return open bus value
		}

		return memory_[mirrored_addr];
	}

	/// Write a byte to RAM
	/// Handles mirroring automatically
	void write(Address address, Byte value) noexcept {
		const Address mirrored_addr = mirror_ram_address(address);

		if (mirrored_addr >= RAM_SIZE) {
			// Invalid RAM access - should not happen with proper mirroring
			return;
		}

		memory_[mirrored_addr] = value;
	}

	/// Get direct access to RAM for debugging
	[[nodiscard]] const std::array<Byte, RAM_SIZE> &get_memory() const noexcept {
		return memory_;
	}

	/// Print RAM contents for debugging
	void debug_print(Address start = 0x0000, std::size_t length = 256) const {
		std::cout << "RAM Dump (starting at $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
				  << start << "):\n";

		for (std::size_t i = 0; i < length && (start + i) < RAM_SIZE; ++i) {
			if (i % 16 == 0) {
				std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (start + i) << ": ";
			}

			std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
					  << static_cast<int>(memory_[start + i]) << " ";

			if ((i + 1) % 16 == 0) {
				std::cout << "\n";
			}
		}
		std::cout << std::dec << "\n"; // Reset to decimal
	}

  private:
	/// 2KB of work RAM
	std::array<Byte, RAM_SIZE> memory_{};
};

} // namespace nes
