#pragma once

#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace nes {

inline bool is_directory_writable(const std::filesystem::path &dir) {
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		return false;
	}

	auto probe_path = dir / ".vibenes_write_test.tmp";
	std::ofstream probe(probe_path, std::ios::binary | std::ios::trunc);
	if (!probe.is_open()) {
		return false;
	}
	probe << "ok";
	probe.close();

	std::filesystem::remove(probe_path, ec);
	return true;
}

inline std::filesystem::path get_pref_base_path() {
	char *pref = SDL_GetPrefPath("VibeNES", "VibeNES");
	if (!pref) {
		return {};
	}
	std::filesystem::path pref_path(pref);
	SDL_free(pref);
	return pref_path;
}

inline std::filesystem::path get_user_documents_directory() {
#if defined(_WIN32)
	char *home_buf = nullptr;
	size_t home_len = 0;
	_dupenv_s(&home_buf, &home_len, "USERPROFILE");
	std::string home_str = home_buf ? home_buf : "";
	free(home_buf);
	if (!home_str.empty()) {
		return std::filesystem::path(home_str) / "Documents";
	}
#else
	const char *home = std::getenv("HOME"); // NOLINT: getenv is standard POSIX usage
	if (home) {
		return std::filesystem::path(home) / "Documents";
	}
#endif
	return {};
}

// Returns the directory containing the running executable.
// Result is cached after first call.
inline const std::filesystem::path &get_exe_dir() {
	static std::filesystem::path dir = []() -> std::filesystem::path {
		const char *base = SDL_GetBasePath();
		if (!base)
			return std::filesystem::current_path();
		return std::filesystem::path(base);
	}();
	return dir;
}

// Portable mode: a "roms/" folder exists next to the executable.
// Installed mode: no such folder exists (standard install layout).
inline bool is_portable_mode() {
	const auto exe_dir = get_exe_dir();
	const auto roms_dir = exe_dir / "roms";

	// Treat as portable only when content sits next to exe AND location is writable.
	// Program Files installs are typically read-only for standard users.
	return std::filesystem::exists(roms_dir) && is_directory_writable(exe_dir);
}

// Default directory to look for NES ROMs.
//   Portable : <exe_dir>/roms
//   Installed: %USERPROFILE%\Documents\VibeNES\roms  (Windows)
//              $HOME/Documents/VibeNES/roms           (others)
inline std::filesystem::path get_roms_directory() {
	const auto exe_dir = get_exe_dir();
	const auto portable_roms = exe_dir / "roms";
	if (is_portable_mode()) {
		return portable_roms;
	}

	const auto docs = get_user_documents_directory();
	if (!docs.empty()) {
		return docs / "VibeNES" / "roms";
	}

	// Fallback: use folder next to exe
	return portable_roms;
}

// Directory where save-state files are written.
//   Portable : <exe_dir>/saves
//   Installed: SDL_GetPrefPath("VibeNES","VibeNES")/saves
//              → %APPDATA%\VibeNES\VibeNES\saves  on Windows
inline std::filesystem::path get_saves_directory() {
	const auto exe_dir = get_exe_dir();
	if (is_portable_mode()) {
		return exe_dir / "saves";
	}

	// Installed mode — use the platform preference directory
	const auto pref = get_pref_base_path();
	if (!pref.empty()) {
		return pref / "saves";
	}
	return exe_dir / "saves"; // safe fallback
}

inline bool copy_directory_tree(const std::filesystem::path &source, const std::filesystem::path &destination) {
	if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) {
		return true;
	}

	std::error_code ec;
	std::filesystem::create_directories(destination, ec);
	if (ec) {
		return false;
	}

	for (const auto &entry : std::filesystem::recursive_directory_iterator(source, ec)) {
		if (ec) {
			return false;
		}

		auto rel = std::filesystem::relative(entry.path(), source, ec);
		if (ec) {
			return false;
		}

		auto target = destination / rel;
		if (entry.is_directory()) {
			std::filesystem::create_directories(target, ec);
			if (ec) {
				return false;
			}
			continue;
		}

		if (entry.is_regular_file()) {
			std::filesystem::create_directories(target.parent_path(), ec);
			if (ec) {
				return false;
			}
			std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::skip_existing, ec);
			if (ec) {
				return false;
			}
		}
	}

	return true;
}

inline bool migrate_packaged_data_to_user_dirs_once() {
	if (is_portable_mode()) {
		return false;
	}

	const auto pref_base = get_pref_base_path();
	if (pref_base.empty()) {
		return false;
	}

	const auto marker = pref_base / "migration_v1.done";
	if (std::filesystem::exists(marker)) {
		return false;
	}

	const auto exe_dir = get_exe_dir();
	const auto packaged_roms = exe_dir / "roms";
	const auto packaged_saves = exe_dir / "saves";

	const bool has_packaged_data = std::filesystem::exists(packaged_roms) || std::filesystem::exists(packaged_saves);
	if (!has_packaged_data) {
		return false;
	}

	const auto user_roms = get_roms_directory();
	const auto user_saves = get_saves_directory();

	const bool roms_ok = copy_directory_tree(packaged_roms, user_roms);
	const bool saves_ok = copy_directory_tree(packaged_saves, user_saves);

	if (roms_ok && saves_ok) {
		std::error_code ec;
		std::filesystem::create_directories(marker.parent_path(), ec);
		std::ofstream marker_file(marker, std::ios::trunc);
		if (marker_file.is_open()) {
			marker_file << "migration_v1_complete\n";
		}
		return true;
	}

	return false;
}

} // namespace nes
