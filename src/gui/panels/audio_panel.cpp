#include "gui/panels/audio_panel.hpp"
#include "gui/style/retro_theme.hpp"
#include <imgui.h>

namespace nes {

using gui::RetroTheme;

AudioPanel::AudioPanel()
	: volume_slider_(1.0f), audio_enabled_(true), audio_level_(0.0f), peak_hold_counter_(0), first_render_(true) {
}

void AudioPanel::render(SystemBus *bus) {
	if (!bus) {
		ImGui::TextColored(RetroTheme::NES_RED, "No system bus connected");
		return;
	}

	render_controls(bus);
	ImGui::Separator();
	render_level_meter(bus);
	ImGui::Separator();
	render_info(bus);
}

void AudioPanel::render_controls(SystemBus *bus) {
	// Enable/Disable audio
	// Checkbox controls whether audio should play when emulation runs
	// Don't sync with backend state - checkbox is the user's preference

	// On first render, sync backend with checkbox default state
	if (first_render_) {
		first_render_ = false;
		if (audio_enabled_) {
			bus->start_audio();
		}
	}

	if (ImGui::Checkbox("Enable Audio", &audio_enabled_)) {
		if (audio_enabled_) {
			bus->start_audio();
		} else {
			bus->stop_audio();
		}
	}

	// Volume slider
	float current_volume = bus->get_audio_volume();
	volume_slider_ = current_volume;

	ImGui::Text("Volume:");
	if (ImGui::SliderFloat("##volume", &volume_slider_, 0.0f, 1.0f, "%.2f")) {
		bus->set_audio_volume(volume_slider_);
	}

	// Quick volume buttons
	ImGui::SameLine();
	if (ImGui::SmallButton("Mute")) {
		volume_slider_ = 0.0f;
		bus->set_audio_volume(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("50%")) {
		volume_slider_ = 0.5f;
		bus->set_audio_volume(0.5f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("100%")) {
		volume_slider_ = 1.0f;
		bus->set_audio_volume(1.0f);
	}
}

void AudioPanel::render_level_meter(SystemBus *bus) {
	(void)bus; // Reserved for future use to query actual audio levels
	ImGui::Text("Audio Level");

	// Get current audio level from buffer size (proxy for activity)
	// In a real implementation, we'd track actual sample levels
	float level = audio_enabled_ ? volume_slider_ * 0.7f : 0.0f;

	// Smooth the level meter
	audio_level_ = audio_level_ * 0.9f + level * 0.1f;

	// Draw level meter as a progress bar
	ImVec4 meter_color = audio_level_ > 0.8f   ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)	 // Red (clipping)
						 : audio_level_ > 0.6f ? ImVec4(1.0f, 1.0f, 0.3f, 1.0f)	 // Yellow
											   : ImVec4(0.3f, 1.0f, 0.3f, 1.0f); // Green

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, meter_color);
	ImGui::ProgressBar(audio_level_, ImVec2(-1, 20), "");
	ImGui::PopStyleColor();

	// Show peak indicator
	if (audio_level_ > 0.95f) {
		ImGui::SameLine();
		ImGui::TextColored(RetroTheme::NES_RED, "PEAK");
	}
}

void AudioPanel::render_info(SystemBus *bus) {
	(void)bus; // Reserved for future use to query actual audio state
	ImGui::Text("Audio Information");

	// Status
	ImGui::Text("Status: %s", audio_enabled_ ? "Playing" : "Stopped");

	// Sample rate (hardcoded for now, could query AudioBackend)
	ImGui::Text("Sample Rate: 44100 Hz");
	ImGui::Text("Buffer Size: 1024 samples");

	// Format
	ImGui::Text("Format: 32-bit Float Stereo");

	// APU info
	ImGui::TextColored(RetroTheme::NES_YELLOW, "NES APU:");
	ImGui::Text("  CPU Rate: ~1.789 MHz");
	ImGui::Text("  Downsampling: ~40.58:1");
}

} // namespace nes
