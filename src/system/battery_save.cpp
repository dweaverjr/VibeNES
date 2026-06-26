#include "system/battery_save.hpp"
#include "cartridge/cartridge.hpp"
#include "core/types.hpp"
#include "core/user_paths.hpp"

#include <fstream>
#include <iostream>
#include <vector>

namespace nes {

BatterySaveManager::BatterySaveManager(Cartridge *cartridge) : cartridge_(cartridge) {
	directory_ = get_battery_directory();
}

void BatterySaveManager::set_directory(std::filesystem::path dir) {
	directory_ = std::move(dir);
}

std::filesystem::path BatterySaveManager::file_path_for_current_rom() const {
	if (!cartridge_ || !cartridge_->is_loaded()) {
		return {};
	}

	// Use only the ROM's leaf name (stem) so .sav paths are stable across
	// machines and match the save-state slot naming convention.
	std::filesystem::path rom_path(cartridge_->get_rom_filename());
	std::string stem = rom_path.stem().string();
	if (stem.empty()) {
		stem = "default";
	}
	return directory_ / (stem + ".sav");
}

void BatterySaveManager::load_for_current_rom() {
	seconds_since_flush_ = 0.0;

	if (!cartridge_ || !cartridge_->is_loaded() || !cartridge_->has_battery_ram()) {
		return;
	}

	const auto path = file_path_for_current_rom();
	if (path.empty() || !std::filesystem::exists(path)) {
		return; // No prior save — start with the mapper's fresh PRG-RAM.
	}

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cerr << "Battery save: failed to open " << path.string() << std::endl;
		return;
	}

	const std::streamsize size = file.tellg();
	if (size <= 0) {
		return;
	}
	file.seekg(0, std::ios::beg);

	std::vector<Byte> data(static_cast<std::size_t>(size));
	if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
		std::cerr << "Battery save: failed to read " << path.string() << std::endl;
		return;
	}

	cartridge_->load_battery_ram(data);
	cartridge_->clear_battery_ram_dirty();
}

bool BatterySaveManager::flush(bool force) {
	if (!cartridge_ || !cartridge_->is_loaded() || !cartridge_->has_battery_ram()) {
		return false;
	}
	if (!force && !cartridge_->is_battery_ram_dirty()) {
		return false;
	}

	const auto data = cartridge_->get_battery_ram();
	if (data.empty()) {
		return false;
	}

	const auto path = file_path_for_current_rom();
	if (path.empty()) {
		return false;
	}

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		std::cerr << "Battery save: cannot create directory " << path.parent_path().string() << ": " << ec.message()
				  << std::endl;
		return false;
	}

	// Write to a temp file then rename so a crash mid-write can't corrupt an
	// existing good save.
	const auto tmp_path = path.string() + ".tmp";
	{
		std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
		if (!file.is_open()) {
			std::cerr << "Battery save: cannot write " << tmp_path << std::endl;
			return false;
		}
		file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
		if (!file) {
			std::cerr << "Battery save: write failed for " << tmp_path << std::endl;
			return false;
		}
	}

	std::filesystem::rename(tmp_path, path, ec);
	if (ec) {
		// rename can fail across some filesystems; fall back to copy + remove.
		ec.clear();
		std::filesystem::copy_file(tmp_path, path, std::filesystem::copy_options::overwrite_existing, ec);
		std::error_code remove_ec;
		std::filesystem::remove(tmp_path, remove_ec);
		if (ec) {
			std::cerr << "Battery save: failed to finalize " << path.string() << ": " << ec.message() << std::endl;
			return false;
		}
	}

	cartridge_->clear_battery_ram_dirty();
	seconds_since_flush_ = 0.0;
	return true;
}

void BatterySaveManager::update(double delta_seconds) {
	// Periodic crash-safety flush: mirror the cartridge SRAM to the .sav at most
	// every kFlushIntervalSeconds, and only when it actually changed. The save's
	// authoritative persistence still happens via flush() on reset/unload/exit.
	if (delta_seconds > 0.0) {
		seconds_since_flush_ += delta_seconds;
	}
	if (seconds_since_flush_ >= kFlushIntervalSeconds) {
		seconds_since_flush_ = 0.0;
		flush(false);
	}
}

} // namespace nes
