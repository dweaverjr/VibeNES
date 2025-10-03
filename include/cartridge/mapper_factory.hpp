#pragma once

#include "cartridge/mappers/mapper.hpp"
#include "cartridge/rom_loader.hpp"
#include <memory>

namespace nes {

/**
 * MapperFactory - Creates appropriate mapper instances based on ROM data
 *
 * This factory handles the creation of mapper objects based on the mapper ID
 * found in the ROM header. It abstracts the mapper selection logic away from
 * the cartridge class.
 */
class MapperFactory {
  public:
	/**
	 * Create a mapper instance based on ROM data
	 *
	 * @param rom_data The ROM data containing mapper ID and other info
	 * @return Unique pointer to the created mapper, or nullptr if unsupported
	 */
	static std::unique_ptr<Mapper> create_mapper(const RomData &rom_data);

  private:
	// Helper to determine mirroring mode from ROM data
	static Mapper::Mirroring get_mirroring_mode(const RomData &rom_data);
};

} // namespace nes
