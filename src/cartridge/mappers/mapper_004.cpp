#include "cartridge/mappers/mapper_004.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace nes {

Mapper004::Mapper004(std::vector<Byte> prg_rom, std::vector<Byte> chr_mem, Mirroring mirroring, bool has_prg_ram,
					 bool chr_is_ram, bool battery_backed)
	: prg_rom_(std::move(prg_rom)), chr_mem_(std::move(chr_mem)), initial_mirroring_(mirroring),
	  has_prg_ram_(has_prg_ram), chr_is_ram_(chr_is_ram), battery_backed_(battery_backed), bank_select_(0), banks_{},
	  mirroring_(false), prg_ram_protect_(0x80), // PRG RAM enabled by default
	  irq_latch_(0), irq_counter_(0), irq_reload_(false), irq_enabled_(false) {

	// Initialize PRG RAM if needed (8KB)
	if (has_prg_ram_) {
		prg_ram_.resize(8192, 0x00);
	}

	// If CHR is RAM and the vector is empty, allocate 8KB CHR RAM.
	// Also guard against a malformed ROM declaring CHR ROM but providing no
	// data: an empty CHR vector makes the 1KB bank count 0, and the
	// `(bank_count - 1)` masks below would wrap to SIZE_MAX. Allocate a
	// minimal 8KB region so bank math stays well-defined.
	if (chr_mem_.empty()) {
		chr_mem_.resize(8192, 0x00);
		chr_is_ram_ = true;
	}

	// Initialize bank registers to power-on state
	// R0-R5: CHR banks (set to 0-5)
	// R6-R7: PRG banks (R6=0, R7=1)
	for (std::size_t i = 0; i < 6; ++i) {
		banks_[i] = static_cast<Byte>(i);
	}
	banks_[6] = 0; // First 8KB PRG bank
	banks_[7] = 1; // Second 8KB PRG bank

	update_bank_maps();
}

void Mapper004::reset() {
	// Reset to power-on state
	bank_select_ = 0;
	std::fill(banks_.begin(), banks_.end(), 0);

	// Initialize bank registers
	for (std::size_t i = 0; i < 6; ++i) {
		banks_[i] = static_cast<Byte>(i);
	}
	banks_[6] = 0;
	banks_[7] = 1;

	mirroring_ = (initial_mirroring_ == Mirroring::Horizontal);
	prg_ram_protect_ = 0x80; // PRG RAM enabled

	// Reset IRQ system
	irq_latch_ = 0;
	irq_counter_ = 0;
	irq_reload_ = false;
	irq_enabled_ = false;
	irq_pending_ = false;

	// NOTE: Battery-backed PRG-RAM is intentionally NOT cleared on reset. Real
	// hardware preserves cartridge save RAM across the reset button; only a cold
	// boot (constructor) starts it zeroed, and the .sav is restored on ROM load.

	update_bank_maps();
}

void Mapper004::update_bank_maps() {
	// Re-resolve each fixed-size slot through the existing bank-offset logic.
	// Called only when bank select/data registers change, so the per-byte hot
	// path is a single pointer lookup with no mode checks or bounds tests.
	for (std::size_t slot = 0; slot < prg_map_.size(); ++slot) {
		std::size_t offset = get_prg_bank_offset(static_cast<Address>(0x8000 + slot * 0x2000));
		prg_map_[slot] = (offset + 0x2000 <= prg_rom_.size()) ? prg_rom_.data() + offset : OPEN_BUS_PAGE.data();
	}
	for (std::size_t slot = 0; slot < chr_map_.size(); ++slot) {
		std::size_t offset = get_chr_bank_offset(static_cast<Address>(slot * 1024));
		chr_map_[slot] = (offset + 1024 <= chr_mem_.size()) ? chr_mem_.data() + offset : OPEN_BUS_PAGE.data();
	}
}

Byte Mapper004::cpu_read(Address address) const {
	// PRG RAM: $6000-$7FFF (8KB)
	if (address >= 0x6000 && address < 0x8000) {
		if (has_prg_ram_ && is_prg_ram_enabled()) {
			return prg_ram_[address - 0x6000];
		}
		return 0xFF; // Open bus if RAM disabled or not present
	}

	// PRG ROM: $8000-$FFFF
	if (address >= 0x8000) {
		// Cached 8KB bank pointers
		return prg_map_[(address >> 13) & 0x03][address & 0x1FFF];
	}

	return 0xFF; // Open bus
}

void Mapper004::cpu_write(Address address, Byte value) {
	// PRG RAM: $6000-$7FFF (8KB)
	if (address >= 0x6000 && address < 0x8000) {
		if (has_prg_ram_ && is_prg_ram_enabled() && is_prg_ram_writable()) {
			prg_ram_[address - 0x6000] = value;
			prg_ram_dirty_ = true;
		}
		return;
	}

	// MMC3 Register writes: $8000-$FFFF
	if (address >= 0x8000) {
		if (address <= 0x9FFF) {
			// $8000-$9FFF: Bank Select and Bank Data
			if ((address & 0x0001) == 0) {
				// Even addresses: Bank Select ($8000)
				bank_select_ = value;
			} else {
				// Odd addresses: Bank Data ($8001)
				Byte bank_reg = get_selected_bank_register();
				if (bank_reg < 8) {
					banks_[bank_reg] = value;
				}
			}
			update_bank_maps();
		} else if (address <= 0xBFFF) {
			// $A000-$BFFF: Mirroring and PRG RAM Protection
			if ((address & 0x0001) == 0) {
				// Even addresses: Mirroring ($A000)
				mirroring_ = (value & 0x01) != 0;
			} else {
				// Odd addresses: PRG RAM Protect ($A001)
				prg_ram_protect_ = value;
			}
		} else if (address <= 0xDFFF) {
			// $C000-$DFFF: IRQ Latch and IRQ Reload
			if ((address & 0x0001) == 0) {
				// Even addresses: IRQ Latch ($C000)
				irq_latch_ = value;
			} else {
				// Odd addresses: IRQ Reload ($C001)
				// Writing here clears the counter and sets reload flag
				// The counter will reload on the NEXT A12 clock
				irq_counter_ = 0;
				irq_reload_ = true;
			}
		} else {
			// $E000-$FFFF: IRQ Disable and IRQ Enable
			if ((address & 0x0001) == 0) {
				// Even addresses: IRQ Disable ($E000)
				irq_enabled_ = false;
				irq_pending_ = false; // Also acknowledge any pending interrupts
			} else {
				// Odd addresses: IRQ Enable ($E001)
				irq_enabled_ = true;
			}
		}
		return;
	}
}

Byte Mapper004::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF;
	}

	// Cached 1KB bank pointers
	return chr_map_[address >> 10][address & 0x03FF];
}

void Mapper004::ppu_write(Address address, Byte value) {
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

void Mapper004::ppu_a12_toggle() {
	// Called by PPU on A12 rising edge after the low-time filter has passed.
	// Each call represents one validated scanline-counter clock.
	clock_irq_counter();
}

std::size_t Mapper004::get_prg_bank_offset(Address address) const {
	std::size_t bank_8kb_count = get_prg_8kb_bank_count();
	bool prg_mode = get_prg_bank_mode();

	if (address < 0xA000) {
		// $8000-$9FFF: 8KB bank (R6 or second-to-last depending on mode)
		Byte bank = prg_mode ? static_cast<Byte>(bank_8kb_count - 2) : banks_[6];
		bank &= static_cast<Byte>(bank_8kb_count - 1); // Mask to valid range
		return (bank * 8192) + (address - 0x8000);
	} else if (address < 0xC000) {
		// $A000-$BFFF: 8KB bank (R7)
		Byte bank = banks_[7] & static_cast<Byte>(bank_8kb_count - 1);
		return (bank * 8192) + (address - 0xA000);
	} else if (address < 0xE000) {
		// $C000-$DFFF: 8KB bank (R6 or second-to-last depending on mode)
		Byte bank = prg_mode ? banks_[6] : static_cast<Byte>(bank_8kb_count - 2);
		bank &= static_cast<Byte>(bank_8kb_count - 1);
		return (bank * 8192) + (address - 0xC000);
	} else {
		// $E000-$FFFF: 8KB bank (always last bank)
		std::size_t last_bank = bank_8kb_count - 1;
		return (last_bank * 8192) + (address - 0xE000);
	}
}

std::size_t Mapper004::get_chr_bank_offset(Address address) const {
	bool chr_mode = get_chr_bank_mode();
	std::size_t bank_1kb_count = get_chr_1kb_bank_count();

	if (!chr_mode) {
		// Normal mode: R0/R1 control $0000-$0FFF, R2-R5 control $1000-$1FFF
		if (address < 0x0800) {
			// $0000-$07FF: 2KB bank (R0, even numbers only)
			// R0/R1 values are 1KB bank numbers forced to even alignment for 2KB regions
			Byte bank = banks_[0] & 0xFE;				   // Clear lowest bit for 2KB alignment
			bank &= static_cast<Byte>(bank_1kb_count - 1); // Mask to full CHR address space
			return (bank * 1024) + address;
		} else if (address < 0x1000) {
			// $0800-$0FFF: 2KB bank (R1, even numbers only)
			Byte bank = banks_[1] & 0xFE;
			bank &= static_cast<Byte>(bank_1kb_count - 1); // Mask to full CHR address space
			return (bank * 1024) + (address - 0x0800);
		} else if (address < 0x1400) {
			// $1000-$13FF: 1KB bank (R2)
			Byte bank = banks_[2] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x1000);
		} else if (address < 0x1800) {
			// $1400-$17FF: 1KB bank (R3)
			Byte bank = banks_[3] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x1400);
		} else if (address < 0x1C00) {
			// $1800-$1BFF: 1KB bank (R4)
			Byte bank = banks_[4] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x1800);
		} else {
			// $1C00-$1FFF: 1KB bank (R5)
			Byte bank = banks_[5] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x1C00);
		}
	} else {
		// Inverted mode: R2-R5 control $0000-$0FFF, R0/R1 control $1000-$1FFF
		if (address < 0x0400) {
			// $0000-$03FF: 1KB bank (R2)
			Byte bank = banks_[2] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + address;
		} else if (address < 0x0800) {
			// $0400-$07FF: 1KB bank (R3)
			Byte bank = banks_[3] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x0400);
		} else if (address < 0x0C00) {
			// $0800-$0BFF: 1KB bank (R4)
			Byte bank = banks_[4] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x0800);
		} else if (address < 0x1000) {
			// $0C00-$0FFF: 1KB bank (R5)
			Byte bank = banks_[5] & static_cast<Byte>(bank_1kb_count - 1);
			return (bank * 1024) + (address - 0x0C00);
		} else if (address < 0x1800) {
			// $1000-$17FF: 2KB bank (R0, even numbers only)
			Byte bank = banks_[0] & 0xFE;
			bank &= static_cast<Byte>(bank_1kb_count - 1); // Mask to full CHR address space
			return (bank * 1024) + (address - 0x1000);
		} else {
			// $1800-$1FFF: 2KB bank (R1, even numbers only)
			Byte bank = banks_[1] & 0xFE;
			bank &= static_cast<Byte>(bank_1kb_count - 1); // Mask to full CHR address space
			return (bank * 1024) + (address - 0x1800);
		}
	}
}

Mapper::Mirroring Mapper004::get_mirroring() const noexcept {
	// MMC3 can override mirroring dynamically
	return mirroring_ ? Mirroring::Horizontal : Mirroring::Vertical;
}

void Mapper004::clock_irq_counter() {
	// MMC3 IRQ counter behavior (Sharp variant)
	// Hardware-verified behavior:
	// 1. If counter is 0 OR reload flag is set, reload counter from latch
	// 2. Otherwise, decrement counter
	// 3. After reload/decrement, check if counter==0 and fire IRQ if enabled

	if (irq_counter_ == 0 || irq_reload_) {
		// Reload counter from latch
		irq_counter_ = irq_latch_;
		irq_reload_ = false;
	} else {
		// Decrement counter
		irq_counter_--;
	}

	// Check if counter is now 0 and fire IRQ if enabled
	// This check happens AFTER the reload/decrement above
	if (irq_counter_ == 0 && irq_enabled_) {
		irq_pending_ = true;
	}

	update_irq_line();
}

void Mapper004::update_irq_line() {
	// This would typically signal the CPU about the IRQ
	// For now, we'll just track the pending state
	// The actual IRQ line connection would be handled by the cartridge/bus system
	if (irq_pending_) {
		// std::cout << "MMC3 IRQ triggered" << std::endl;
		// In a full implementation, this would set the CPU's IRQ line
	}
}

void Mapper004::serialize_state(std::vector<Byte> &buffer) const {
	// Serialize PRG RAM if present (8KB)
	if (has_prg_ram_) {
		buffer.insert(buffer.end(), prg_ram_.begin(), prg_ram_.end());
	}

	// Serialize CHR memory if it's RAM
	if (chr_is_ram_) {
		buffer.insert(buffer.end(), chr_mem_.begin(), chr_mem_.end());
	}

	// Serialize MMC3 registers
	buffer.push_back(bank_select_);

	// Serialize all 8 bank registers
	for (Byte bank : banks_) {
		buffer.push_back(bank);
	}

	buffer.push_back(mirroring_ ? 1 : 0);
	buffer.push_back(prg_ram_protect_);

	// Serialize IRQ system state
	buffer.push_back(irq_latch_);
	buffer.push_back(irq_counter_);
	buffer.push_back(irq_reload_ ? 1 : 0);
	buffer.push_back(irq_enabled_ ? 1 : 0);
	buffer.push_back(irq_pending_ ? 1 : 0);
}

void Mapper004::deserialize_state(const std::vector<Byte> &buffer, size_t &offset) {
	// Deserialize PRG RAM if present (8KB)
	if (has_prg_ram_) {
		for (size_t i = 0; i < prg_ram_.size() && offset < buffer.size(); ++i) {
			prg_ram_[i] = buffer[offset++];
		}
	}

	// Deserialize CHR memory if it's RAM
	if (chr_is_ram_) {
		for (size_t i = 0; i < chr_mem_.size() && offset < buffer.size(); ++i) {
			chr_mem_[i] = buffer[offset++];
		}
	}

	// Deserialize MMC3 registers
	// Remaining fixed-size fields: bank_select (1) + 8 bank registers +
	// mirroring (1) + prg_ram_protect (1) + 5 IRQ fields = 16 bytes.
	if (offset + 16 > buffer.size()) {
		throw std::runtime_error("save state: unexpected end of buffer (MMC3)");
	}
	bank_select_ = buffer[offset++];

	// Deserialize all 8 bank registers
	for (size_t i = 0; i < 8 && offset < buffer.size(); ++i) {
		banks_[i] = buffer[offset++];
	}

	mirroring_ = buffer[offset++] != 0;
	prg_ram_protect_ = buffer[offset++];

	// Deserialize IRQ system state
	irq_latch_ = buffer[offset++];
	irq_counter_ = buffer[offset++];
	irq_reload_ = buffer[offset++] != 0;
	irq_enabled_ = buffer[offset++] != 0;
	irq_pending_ = buffer[offset++] != 0;

	update_bank_maps();
}

} // namespace nes
