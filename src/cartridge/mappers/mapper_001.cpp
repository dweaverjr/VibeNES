#include "cartridge/mappers/mapper_001.hpp"
#include <algorithm>

namespace nes {

Mapper001::Mapper001(std::vector<Byte> prg_rom, std::vector<Byte> chr_mem, Mirroring mirroring, bool has_prg_ram,
					 bool chr_is_ram)
	: prg_rom_(std::move(prg_rom)), chr_mem_(std::move(chr_mem)), initial_mirroring_(mirroring),
	  has_prg_ram_(has_prg_ram), chr_is_ram_(chr_is_ram), shift_register_(0x10), // Initialize with bit 4 set
	  shift_count_(0), control_register_(0x0C),									 // Default: last bank fixed, 8KB CHR
	  chr_bank_0_(0), chr_bank_1_(0), prg_bank_(0), prg_ram_enabled_(true), cpu_cycle_counter_(0),
	  last_write_cycle_(0) {

	// Initialize PRG RAM if needed (8KB)
	if (has_prg_ram_) {
		prg_ram_.resize(8192, 0x00);
	}

	// If CHR is RAM and the vector is empty, allocate 8KB CHR RAM
	if (chr_is_ram_ && chr_mem_.empty()) {
		chr_mem_.resize(8192, 0x00);
	}
}

void Mapper001::reset() {
	// On power-up, MMC1 sets control to $0C (last bank fixed)
	shift_register_ = 0x10;
	shift_count_ = 0;
	control_register_ = 0x0C;
	chr_bank_0_ = 0;
	chr_bank_1_ = 0;
	prg_bank_ = 0;
	prg_ram_enabled_ = true;
	cpu_cycle_counter_ = 0;
	last_write_cycle_ = 0;

	// Clear PRG RAM
	if (has_prg_ram_) {
		std::fill(prg_ram_.begin(), prg_ram_.end(), 0x00);
	}
}

Byte Mapper001::cpu_read(Address address) const {
	// PRG RAM: $6000-$7FFF (8KB)
	if (address >= 0x6000 && address < 0x8000) {
		if (has_prg_ram_ && prg_ram_enabled_) {
			return prg_ram_[address - 0x6000];
		}
		return 0xFF; // Open bus if RAM disabled or not present
	}

	// PRG ROM: $8000-$FFFF
	if (address >= 0x8000) {
		std::size_t rom_offset = get_prg_bank_offset(address);
		if (rom_offset < prg_rom_.size()) {
			return prg_rom_[rom_offset];
		}
	}

	return 0xFF; // Open bus
}

void Mapper001::cpu_write(Address address, Byte value) {
	// PRG RAM: $6000-$7FFF (8KB)
	if (address >= 0x6000 && address < 0x8000) {
		if (has_prg_ram_ && prg_ram_enabled_) {
			prg_ram_[address - 0x6000] = value;
		}
		return;
	}

	// MMC1 Register writes: $8000-$FFFF
	if (address >= 0x8000) {
		// Consecutive-write filter: ignore writes on back-to-back CPU cycles.
		// RMW instructions write the old value then new value on consecutive
		// cycles; real MMC1 hardware only processes the first write.
		if (cpu_cycle_counter_ == last_write_cycle_ + 1 && last_write_cycle_ != 0) {
			last_write_cycle_ = cpu_cycle_counter_;
			return; // Ignore this consecutive write
		}
		last_write_cycle_ = cpu_cycle_counter_;
		write_shift_register(address, value);
		return;
	}
}

Byte Mapper001::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF;
	}

	std::size_t chr_offset = get_chr_bank_offset(address);
	if (chr_offset < chr_mem_.size()) {
		return chr_mem_[chr_offset];
	}

	return 0xFF;
}

void Mapper001::ppu_write(Address address, Byte value) {
	if (!is_chr_address(address)) {
		return;
	}

	// Only allow writes if CHR is RAM
	if (chr_is_ram_) {
		std::size_t chr_offset = get_chr_bank_offset(address);
		if (chr_offset < chr_mem_.size()) {
			chr_mem_[chr_offset] = value;
		}
	}
}

void Mapper001::write_shift_register(Address address, Byte value) {
	// Bit 7 set = reset shift register
	if (value & 0x80) {
		shift_register_ = 0x10; // Reset to bit 4 set
		shift_count_ = 0;
		control_register_ |= 0x0C; // Set bits 2-3 (fixes last bank)
		return;
	}

	// Shift in bit 0 of value
	shift_register_ >>= 1;
	shift_register_ |= (value & 0x01) << 4;
	shift_count_++;

	// After 5 writes, load the register
	if (shift_count_ >= 5) {
		load_register(address);
		shift_register_ = 0x10; // Reset shift register
		shift_count_ = 0;
	}
}

void Mapper001::load_register(Address address) {
	Byte register_value = shift_register_ & 0x1F; // Only bottom 5 bits

	// Determine which register to load based on address
	if (address >= 0x8000 && address < 0xA000) {
		// Control register ($8000-$9FFF)
		control_register_ = register_value;
	} else if (address >= 0xA000 && address < 0xC000) {
		// CHR bank 0 ($A000-$BFFF)
		chr_bank_0_ = register_value;
	} else if (address >= 0xC000 && address < 0xE000) {
		// CHR bank 1 ($C000-$DFFF)
		chr_bank_1_ = register_value;
	} else if (address >= 0xE000) {
		// PRG bank ($E000-$FFFF)
		prg_bank_ = register_value & 0x0F;				 // Bottom 4 bits = bank number
		prg_ram_enabled_ = (register_value & 0x10) == 0; // Bit 4 = 0 enables RAM
	}
}

std::size_t Mapper001::get_prg_bank_offset(Address address) const {
	Byte mode = get_prg_bank_mode();
	std::size_t bank_16kb = prg_rom_.size() / 16384; // Number of 16KB banks

	if (mode <= 1) {
		// 32KB mode: switch entire 32KB
		Byte bank_32kb = (prg_bank_ >> 1) & ((bank_16kb / 2) - 1);
		return (bank_32kb * 32768) + (address - 0x8000);
	} else if (mode == 2) {
		// Fix first bank at $8000, switch 16KB bank at $C000
		if (address < 0xC000) {
			return address - 0x8000; // First 16KB bank (fixed to bank 0)
		} else {
			Byte bank = prg_bank_ & (bank_16kb - 1);
			return (bank * 16384) + (address - 0xC000);
		}
	} else {
		// mode == 3: Fix last bank at $C000, switch 16KB bank at $8000
		if (address < 0xC000) {
			Byte bank = prg_bank_ & (bank_16kb - 1);
			return (bank * 16384) + (address - 0x8000);
		} else {
			// Last 16KB bank (fixed)
			std::size_t last_bank = bank_16kb - 1;
			return (last_bank * 16384) + (address - 0xC000);
		}
	}
}

std::size_t Mapper001::get_chr_bank_offset(Address address) const {
	bool mode_4kb = get_chr_bank_mode();
	std::size_t bank_4kb_count = chr_mem_.size() / 4096;

	if (!mode_4kb) {
		// 8KB mode: switch entire 8KB
		Byte bank_8kb = (chr_bank_0_ >> 1) & ((bank_4kb_count / 2) - 1);
		return (bank_8kb * 8192) + address;
	} else {
		// 4KB mode: two separate 4KB banks
		if (address < 0x1000) {
			// $0000-$0FFF: use chr_bank_0
			Byte bank = chr_bank_0_ & (bank_4kb_count - 1);
			return (bank * 4096) + address;
		} else {
			// $1000-$1FFF: use chr_bank_1
			Byte bank = chr_bank_1_ & (bank_4kb_count - 1);
			return (bank * 4096) + (address - 0x1000);
		}
	}
}

Mapper::Mirroring Mapper001::get_mirroring() const noexcept {
	Byte mode = get_mirroring_mode();

	switch (mode) {
	case 0:
		return Mirroring::SingleScreenLow;
	case 1:
		return Mirroring::SingleScreenHigh;
	case 2:
		return Mirroring::Vertical;
	case 3:
		return Mirroring::Horizontal;
	default:
		return initial_mirroring_; // Fallback
	}
}

// Save state serialization
void Mapper001::serialize_state(std::vector<uint8_t> &buffer) const {
	// PRG RAM (8KB if present)
	if (has_prg_ram_) {
		buffer.insert(buffer.end(), prg_ram_.begin(), prg_ram_.end());
	}

	// CHR RAM (if CHR is RAM, not ROM)
	if (chr_is_ram_) {
		buffer.insert(buffer.end(), chr_mem_.begin(), chr_mem_.end());
	}

	// MMC1 registers
	buffer.push_back(shift_register_);
	buffer.push_back(shift_count_);
	buffer.push_back(control_register_);
	buffer.push_back(chr_bank_0_);
	buffer.push_back(chr_bank_1_);
	buffer.push_back(prg_bank_);
	buffer.push_back(prg_ram_enabled_ ? 1 : 0);
}

void Mapper001::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// PRG RAM (8KB if present)
	if (has_prg_ram_) {
		std::copy(buffer.begin() + offset, buffer.begin() + offset + prg_ram_.size(), prg_ram_.begin());
		offset += prg_ram_.size();
	}

	// CHR RAM (if CHR is RAM, not ROM)
	if (chr_is_ram_) {
		std::copy(buffer.begin() + offset, buffer.begin() + offset + chr_mem_.size(), chr_mem_.begin());
		offset += chr_mem_.size();
	}

	// MMC1 registers
	shift_register_ = buffer[offset++];
	shift_count_ = buffer[offset++];
	control_register_ = buffer[offset++];
	chr_bank_0_ = buffer[offset++];
	chr_bank_1_ = buffer[offset++];
	prg_bank_ = buffer[offset++];
	prg_ram_enabled_ = buffer[offset++] != 0;
}

} // namespace nes
