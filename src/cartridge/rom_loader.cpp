#include "cartridge/rom_loader.hpp"
#include <fstream>
#include <iostream>

namespace nes {

RomData RomLoader::load_rom(const std::string &filepath) {
	RomData rom_data{};
	rom_data.filename = filepath;
	rom_data.valid = false;

	// Read entire file
	std::vector<Byte> file_data = read_file(filepath);
	if (file_data.empty()) {
		std::cerr << "Failed to read ROM file: " << filepath << std::endl;
		return rom_data;
	}

	// Validate minimum size
	if (file_data.size() < INES_HEADER_SIZE) {
		std::cerr << "File too small to be valid iNES ROM: " << filepath << std::endl;
		return rom_data;
	}

	// Extract and validate header
	std::vector<Byte> header(file_data.begin(), file_data.begin() + INES_HEADER_SIZE);
	if (!validate_header(header)) {
		std::cerr << "Invalid iNES header: " << filepath << std::endl;
		return rom_data;
	}

	// Parse header
	rom_data = parse_header(header);
	rom_data.filename = filepath;

	// Calculate expected sizes
	std::size_t expected_prg_size = rom_data.prg_rom_pages * PRG_ROM_PAGE_SIZE;
	std::size_t expected_chr_size = rom_data.chr_rom_pages * CHR_ROM_PAGE_SIZE;
	std::size_t trainer_size = rom_data.trainer_present ? TRAINER_SIZE : 0;
	std::size_t expected_total = INES_HEADER_SIZE + trainer_size + expected_prg_size + expected_chr_size;

	if (file_data.size() < expected_total) {
		std::cerr << "ROM file smaller than expected: " << filepath << std::endl;
		std::cerr << "Expected: " << expected_total << " bytes, got: " << file_data.size() << std::endl;
		return rom_data;
	}

	// Extract data sections
	std::size_t offset = INES_HEADER_SIZE;

	// Trainer (if present)
	if (rom_data.trainer_present) {
		rom_data.trainer.assign(file_data.begin() + offset, file_data.begin() + offset + TRAINER_SIZE);
		offset += TRAINER_SIZE;
	}

	// PRG ROM
	if (expected_prg_size > 0) {
		rom_data.prg_rom.assign(file_data.begin() + offset, file_data.begin() + offset + expected_prg_size);
		offset += expected_prg_size;
	}

	// CHR ROM
	if (expected_chr_size > 0) {
		rom_data.chr_rom.assign(file_data.begin() + offset, file_data.begin() + offset + expected_chr_size);
	}

	rom_data.valid = true;

	// Debug output
	std::cout << "Loaded ROM: " << filepath << std::endl;
	std::cout << "  Mapper: " << static_cast<int>(rom_data.mapper_id) << std::endl;
	std::cout << "  PRG ROM: " << static_cast<int>(rom_data.prg_rom_pages) << " x 16KB" << std::endl;
	std::cout << "  CHR ROM: " << static_cast<int>(rom_data.chr_rom_pages) << " x 8KB" << std::endl;
	std::cout << "  Mirroring: " << (rom_data.vertical_mirroring ? "Vertical" : "Horizontal") << std::endl;

	return rom_data;
}

bool RomLoader::is_valid_nes_file(const std::string &filepath) {
	std::vector<Byte> file_data = read_file(filepath);
	if (file_data.size() < INES_HEADER_SIZE) {
		return false;
	}

	std::vector<Byte> header(file_data.begin(), file_data.begin() + INES_HEADER_SIZE);
	return validate_header(header);
}

bool RomLoader::validate_header(const std::vector<Byte> &header) {
	if (header.size() < INES_HEADER_SIZE) {
		return false;
	}

	// Check magic number "NES\x1A"
	for (std::size_t i = 0; i < INES_MAGIC.size(); ++i) {
		if (header[i] != INES_MAGIC[i]) {
			return false;
		}
	}

	return true;
}

RomData RomLoader::parse_header(const std::vector<Byte> &header) {
	RomData rom_data{};

	// Bytes 4-5: ROM sizes
	rom_data.prg_rom_pages = header[4];
	rom_data.chr_rom_pages = header[5];

	// Byte 6: Flags 6
	Byte flags6 = header[6];
	rom_data.vertical_mirroring = (flags6 & 0x01) != 0;
	rom_data.battery_backed_ram = (flags6 & 0x02) != 0;
	rom_data.trainer_present = (flags6 & 0x04) != 0;
	rom_data.four_screen_vram = (flags6 & 0x08) != 0;

	// Byte 7: Flags 7
	Byte flags7 = header[7];

	// Mapper ID: combine lower and upper nibbles
	rom_data.mapper_id = (flags6 >> 4) | (flags7 & 0xF0);

	rom_data.valid = true;
	return rom_data;
}

std::vector<Byte> RomLoader::read_file(const std::string &filepath) {
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open()) {
		return {};
	}

	// Get file size
	file.seekg(0, std::ios::end);
	std::size_t size = static_cast<std::size_t>(file.tellg());
	file.seekg(0, std::ios::beg);

	// Read entire file
	std::vector<Byte> data(size);
	file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

	return data;
}

} // namespace nes
