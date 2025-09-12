#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace nes {

/**
 * Structure representing an iNES ROM file header and data
 */
struct RomData {
	// Header information
	std::uint8_t mapper_id;
	std::uint8_t prg_rom_pages; // 16KB pages
	std::uint8_t chr_rom_pages; // 8KB pages
	bool vertical_mirroring;
	bool battery_backed_ram;
	bool trainer_present;
	bool four_screen_vram;

	// ROM data
	std::vector<Byte> prg_rom; // Program ROM
	std::vector<Byte> chr_rom; // Character ROM
	std::vector<Byte> trainer; // Optional 512-byte trainer

	// File info
	std::string filename;
	bool valid;
};

/**
 * Utility class for loading NES ROM files in iNES format
 */
class RomLoader {
  public:
	/**
	 * Load a ROM file from disk
	 * @param filepath Path to the .nes file
	 * @return RomData structure with loaded ROM data
	 */
	static RomData load_rom(const std::string &filepath);

	/**
	 * Validate that a file appears to be a valid iNES ROM
	 * @param filepath Path to check
	 * @return true if file has valid iNES header
	 */
	static bool is_valid_nes_file(const std::string &filepath);

  private:
	// iNES header constants
	static constexpr std::size_t INES_HEADER_SIZE = 16;
	static constexpr std::size_t TRAINER_SIZE = 512;
	static constexpr std::size_t PRG_ROM_PAGE_SIZE = 16384; // 16KB
	static constexpr std::size_t CHR_ROM_PAGE_SIZE = 8192;	// 8KB

	// iNES magic number "NES\x1A"
	static constexpr std::array<Byte, 4> INES_MAGIC = {0x4E, 0x45, 0x53, 0x1A};

	// Helper functions
	static bool validate_header(const std::vector<Byte> &header);
	static RomData parse_header(const std::vector<Byte> &header);
	static std::vector<Byte> read_file(const std::string &filepath);
};

} // namespace nes
