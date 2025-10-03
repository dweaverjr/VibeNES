#include "cartridge/mapper_factory.hpp"
#include "cartridge/mappers/mapper_000.hpp"
// TODO: Add other mapper headers as they are implemented
// #include "cartridge/mappers/mapper_001.hpp"
// #include "cartridge/mappers/mapper_002.hpp"
// #include "cartridge/mappers/mapper_003.hpp"
// #include "cartridge/mappers/mapper_004.hpp"
#include <iostream>

namespace nes {

std::unique_ptr<Mapper> MapperFactory::create_mapper(const RomData &rom_data) {
	// Get mirroring mode from ROM data
	Mapper::Mirroring mirroring = get_mirroring_mode(rom_data);

	// Create mapper based on ID
	switch (rom_data.mapper_id) {
	case 0:
		// Mapper 0 - NROM (No mapper)
		// Used by: Super Mario Bros, Donkey Kong, Ice Climber, etc.
		return std::make_unique<Mapper000>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

		// TODO: Implement these mappers
		// case 1:
		// 	// Mapper 1 - MMC1 (SxROM)
		// 	// Used by: The Legend of Zelda, Metroid, Mega Man 2, etc.
		// 	std::cout << "Creating Mapper 001 (MMC1)" << std::endl;
		// 	return std::make_unique<Mapper001>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

		// case 2:
		// 	// Mapper 2 - UxROM
		// 	// Used by: Mega Man, Castlevania, Contra, etc.
		// 	std::cout << "Creating Mapper 002 (UxROM)" << std::endl;
		// 	return std::make_unique<Mapper002>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

		// case 3:
		// 	// Mapper 3 - CNROM
		// 	// Used by: Q*bert, Cybernoid, Solomon's Key, etc.
		// 	std::cout << "Creating Mapper 003 (CNROM)" << std::endl;
		// 	return std::make_unique<Mapper003>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

		// case 4:
		// 	// Mapper 4 - MMC3 (TxROM)
		// 	// Used by: Super Mario Bros 2/3, Mega Man 3-6, etc.
		// 	std::cout << "Creating Mapper 004 (MMC3)" << std::endl;
		// 	return std::make_unique<Mapper004>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

	default:
		std::cerr << "Unsupported mapper ID: " << static_cast<int>(rom_data.mapper_id) << std::endl;
		std::cerr << "Currently only Mapper 0 (NROM) is supported." << std::endl;
		std::cerr << "To add support, implement the mapper class and add it to MapperFactory." << std::endl;
		return nullptr;
	}
}

Mapper::Mirroring MapperFactory::get_mirroring_mode(const RomData &rom_data) {
	if (rom_data.four_screen_vram) {
		return Mapper::Mirroring::FourScreen;
	} else if (rom_data.vertical_mirroring) {
		return Mapper::Mirroring::Vertical;
	} else {
		return Mapper::Mirroring::Horizontal;
	}
}

} // namespace nes
