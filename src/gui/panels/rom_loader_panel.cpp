#include "gui/panels/rom_loader_panel.hpp"
#include "cartridge/cartridge.hpp"
#include <filesystem>
#include <imgui.h>
#include <iostream>

namespace nes::gui {

void RomLoaderPanel::render(nes::Cartridge *cartridge) {
	if (!cartridge) {
		return;
	}

	// File browser section
	if (ImGui::CollapsingHeader("File Browser", ImGuiTreeNodeFlags_DefaultOpen)) {
		render_file_browser(cartridge);
	}

	ImGui::Separator();

	// ROM information section
	if (ImGui::CollapsingHeader("ROM Information", ImGuiTreeNodeFlags_DefaultOpen)) {
		render_rom_info(cartridge);
	}

	ImGui::Separator();

	// Load button
	render_load_button(cartridge);
}

void RomLoaderPanel::render_file_browser(nes::Cartridge *cartridge) {
	// Current directory display
	ImGui::Text("Directory: %s", current_directory_.c_str());

	// Create directory if it doesn't exist
	if (!std::filesystem::exists(current_directory_)) {
		if (ImGui::Button("Create ROMs Directory")) {
			try {
				std::filesystem::create_directories(current_directory_);
			} catch (const std::exception &e) {
				std::cerr << "Failed to create directory: " << e.what() << std::endl;
			}
		}
		ImGui::Text("ROMs directory doesn't exist. Click above to create it.");
		return;
	}

	// File list
	if (ImGui::BeginListBox("##files", ImVec2(-1, 200))) {
		try {
			for (const auto &entry : std::filesystem::directory_iterator(current_directory_)) {
				if (entry.is_regular_file()) {
					std::string filename = entry.path().filename().string();

					// Filter by extension
					if (is_nes_file(filename)) {
						bool is_selected = (selected_file_ == entry.path().string());

						if (ImGui::Selectable(filename.c_str(), is_selected)) {
							selected_file_ = entry.path().string();
						}

						// Double-click to load
						if (is_selected && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
							if (cartridge->load_rom(selected_file_)) {
								std::cout << "ROM loaded successfully: " << filename << std::endl;
							}
						}
					}
				}
			}
		} catch (const std::exception &e) {
			ImGui::Text("Error reading directory: %s", e.what());
		}
		ImGui::EndListBox();
	}

	// Selected file display
	if (!selected_file_.empty()) {
		ImGui::Text("Selected: %s", std::filesystem::path(selected_file_).filename().string().c_str());
	}
}

void RomLoaderPanel::render_rom_info(nes::Cartridge *cartridge) {
	if (!cartridge->is_loaded()) {
		ImGui::Text("No ROM loaded");
		return;
	}

	const auto &rom_data = cartridge->get_rom_data();

	ImGui::Text("Filename: %s", std::filesystem::path(rom_data.filename).filename().string().c_str());
	ImGui::Text("Mapper: %d (%s)", cartridge->get_mapper_id(), cartridge->get_mapper_name());
	ImGui::Text("PRG ROM: %d x 16KB (%d bytes)", static_cast<int>(rom_data.prg_rom_pages),
				static_cast<int>(rom_data.prg_rom.size()));
	ImGui::Text("CHR ROM: %d x 8KB (%d bytes)", static_cast<int>(rom_data.chr_rom_pages),
				static_cast<int>(rom_data.chr_rom.size()));

	// Mirroring info
	const char *mirroring_name = "Unknown";
	switch (cartridge->get_mirroring()) {
	case nes::Mapper::Mirroring::Horizontal:
		mirroring_name = "Horizontal";
		break;
	case nes::Mapper::Mirroring::Vertical:
		mirroring_name = "Vertical";
		break;
	case nes::Mapper::Mirroring::SingleScreenLow:
		mirroring_name = "Single Screen Low";
		break;
	case nes::Mapper::Mirroring::SingleScreenHigh:
		mirroring_name = "Single Screen High";
		break;
	case nes::Mapper::Mirroring::FourScreen:
		mirroring_name = "Four Screen";
		break;
	}
	ImGui::Text("Mirroring: %s", mirroring_name);

	// Additional flags
	if (rom_data.battery_backed_ram) {
		ImGui::Text("Battery-backed RAM: Yes");
	}
	if (rom_data.trainer_present) {
		ImGui::Text("Trainer: Yes (%d bytes)", static_cast<int>(rom_data.trainer.size()));
	}
}

void RomLoaderPanel::render_load_button(nes::Cartridge *cartridge) {
	bool can_load = !selected_file_.empty() && is_nes_file(selected_file_);

	if (!can_load) {
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Load ROM", ImVec2(100, 30))) {
		if (cartridge->load_rom(selected_file_)) {
			std::cout << "ROM loaded successfully!" << std::endl;
		} else {
			std::cerr << "Failed to load ROM: " << selected_file_ << std::endl;
		}
	}

	if (!can_load) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	bool can_unload = cartridge->is_loaded();
	if (!can_unload) {
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Unload ROM", ImVec2(100, 30))) {
		cartridge->unload_rom();
		std::cout << "ROM unloaded" << std::endl;
	}

	if (!can_unload) {
		ImGui::EndDisabled();
	}
}

bool RomLoaderPanel::is_nes_file(const std::string &filename) const {
	std::string ext = get_file_extension(filename);
	return ext == ".nes" || ext == ".NES";
}

std::string RomLoaderPanel::get_file_extension(const std::string &filename) const {
	std::size_t dot_pos = filename.find_last_of('.');
	if (dot_pos == std::string::npos) {
		return "";
	}
	return filename.substr(dot_pos);
}

} // namespace nes::gui
