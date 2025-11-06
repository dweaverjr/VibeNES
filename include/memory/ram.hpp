#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <cstdint>
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
		// RAM contents are NOT cleared on reset in real NES hardware
		// Reset only affects CPU state, not memory contents
		// RAM retains its power-on state until explicitly written to
		// (No action needed here)
	}

	void power_on() override {
		// Simulate realistic random power-on state
		// Real NES RAM contains garbage values on power-up

		// Use multiple random sources for better distribution
		std::uint32_t seed = 0x12345678; // Deterministic but complex seed

		for (std::size_t i = 0; i < memory_.size(); ++i) {
			// Multiple LCG iterations with different constants for chaos
			seed = seed * 1664525 + 1013904223; // First LCG
			std::uint32_t temp1 = seed;

			seed = seed * 22695477 + 1; // Second LCG
			std::uint32_t temp2 = seed;

			seed = seed * 48271 + 0; // Third LCG
			std::uint32_t temp3 = seed;

			// Combine multiple noise sources
			std::uint32_t noise = temp1 ^ (temp2 >> 8) ^ (temp3 << 4);
			noise ^= static_cast<std::uint32_t>(i * 0x9E3779B9); // Golden ratio hash
			noise ^= noise >> 16;								 // Additional mixing

			memory_[i] = static_cast<Byte>(noise & 0xFF);
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

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const {
		buffer.insert(buffer.end(), memory_.begin(), memory_.end());
	}

	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
		std::copy(buffer.begin() + offset, buffer.begin() + offset + RAM_SIZE, memory_.begin());
		offset += RAM_SIZE;
	}

  private:
	/// 2KB of work RAM
	std::array<Byte, RAM_SIZE> memory_{};
};

} // namespace nes
