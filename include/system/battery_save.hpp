#pragma once

#include <filesystem>
#include <string>

namespace nes {

class Cartridge;

/**
 * Persists battery-backed PRG-RAM ($6000-$7FFF save RAM) to disk, emulating the
 * on-cartridge battery so games save natively. Files are written as a raw dump
 * of the cartridge's PRG-RAM to <saves>/battery/<rom-stem>.sav — independent of
 * the .vns save-state system.
 *
 * Only cartridges whose iNES header sets the battery flag (and that actually
 * have PRG-RAM, i.e. MMC1/MMC3) participate; everything else is a no-op.
 */
class BatterySaveManager {
  public:
	explicit BatterySaveManager(Cartridge *cartridge);

	void set_directory(std::filesystem::path dir);
	const std::filesystem::path &get_directory() const noexcept {
		return directory_;
	}

	// Restore the .sav file (if present) into the currently loaded cartridge's
	// PRG-RAM. Call after the ROM is loaded AND the system has been reset, so
	// the mapper's power-on clear doesn't wipe the restored contents.
	void load_for_current_rom();

	// Write the .sav if the loaded cart has battery RAM that changed since the
	// last flush. With force=true, writes regardless of the dirty flag.
	// Returns true if a file was written.
	bool flush(bool force = false);

	// Per-frame tick: flushes dirty battery RAM at most every
	// kFlushIntervalSeconds so an unexpected crash loses at most a few seconds.
	// (Power-off persistence proper happens via flush() on reset/unload/exit.)
	void update(double delta_seconds);

  private:
	Cartridge *cartridge_;
	std::filesystem::path directory_;
	double seconds_since_flush_ = 0.0;

	static constexpr double kFlushIntervalSeconds = 5.0;

	// <directory_>/<rom-stem>.sav for the currently loaded ROM, or empty.
	std::filesystem::path file_path_for_current_rom() const;
};

} // namespace nes
