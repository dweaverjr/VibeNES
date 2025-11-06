#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nes {

// Forward declarations
class CPU6502;
class PPU;
class APU;
class SystemBus;
class Cartridge;

// Save state file format version
constexpr uint32_t SAVE_STATE_VERSION = 1;
constexpr char SAVE_STATE_MAGIC[8] = "VIBENES";

// Save state header structure
struct SaveStateHeader {
	char magic[8];		  // "VIBENES\0"
	uint32_t version;	  // File format version
	uint32_t crc32;		  // CRC32 of ROM (for verification)
	uint64_t timestamp;	  // Unix timestamp when saved
	uint32_t data_size;	  // Size of state data in bytes
	uint8_t reserved[32]; // Reserved for future use

	SaveStateHeader();
	bool is_valid() const;
};

// Main save state manager class
class SaveStateManager {
  public:
	SaveStateManager(CPU6502 *cpu, PPU *ppu, APU *apu, SystemBus *bus, Cartridge *cartridge);
	~SaveStateManager() = default;

	// Save/load from file
	bool save_to_file(const std::filesystem::path &path);
	bool load_from_file(const std::filesystem::path &path);

	// Save/load to/from memory buffer
	std::vector<uint8_t> serialize_state();
	bool deserialize_state(const std::vector<uint8_t> &data);

	// Slot-based save/load (1-9)
	bool save_to_slot(int slot);
	bool load_from_slot(int slot);
	std::filesystem::path get_slot_path(int slot) const;
	bool slot_exists(int slot) const;
	std::optional<std::chrono::system_clock::time_point> get_slot_timestamp(int slot) const;

	// Quick save/load
	bool quick_save();
	bool quick_load();

	// Set the base directory for save files
	void set_save_directory(const std::filesystem::path &dir);
	std::filesystem::path get_save_directory() const;

	// Get last error message
	const std::string &get_last_error() const {
		return last_error_;
	}

  private:
	// Component pointers
	CPU6502 *cpu_;
	PPU *ppu_;
	APU *apu_;
	SystemBus *bus_;
	Cartridge *cartridge_;

	// Save directory and state
	std::filesystem::path save_directory_;
	std::string last_error_;

	// Helper methods
	uint32_t calculate_rom_crc32() const;
	bool write_header(std::vector<uint8_t> &buffer, const SaveStateHeader &header);
	bool read_header(const std::vector<uint8_t> &buffer, SaveStateHeader &header);

	// Serialization helpers
	void write_uint8(std::vector<uint8_t> &buffer, uint8_t value);
	void write_uint16(std::vector<uint8_t> &buffer, uint16_t value);
	void write_uint32(std::vector<uint8_t> &buffer, uint32_t value);
	void write_uint64(std::vector<uint8_t> &buffer, uint64_t value);
	void write_bool(std::vector<uint8_t> &buffer, bool value);
	void write_bytes(std::vector<uint8_t> &buffer, const uint8_t *data, size_t size);

	uint8_t read_uint8(const std::vector<uint8_t> &buffer, size_t &offset);
	uint16_t read_uint16(const std::vector<uint8_t> &buffer, size_t &offset);
	uint32_t read_uint32(const std::vector<uint8_t> &buffer, size_t &offset);
	uint64_t read_uint64(const std::vector<uint8_t> &buffer, size_t &offset);
	bool read_bool(const std::vector<uint8_t> &buffer, size_t &offset);
	void read_bytes(const std::vector<uint8_t> &buffer, size_t &offset, uint8_t *data, size_t size);
};

} // namespace nes
