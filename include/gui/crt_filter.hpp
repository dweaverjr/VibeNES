#pragma once

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>

namespace nes::gui {

/// CRT display filter with NTSC aspect ratio correction and visual effects.
/// Renders the NES framebuffer through an OpenGL shader to simulate CRT display.
class CRTFilter {
  public:
	CRTFilter();
	~CRTFilter();

	/// Initialize GL resources. Call after the GL context is created.
	bool initialize();

	/// Release all GL resources.
	void shutdown();

	/// Apply CRT filter to the input NES framebuffer texture.
	/// @param input_texture  Raw 256x240 NES framebuffer texture
	/// @param output_width   Desired output width in pixels
	/// @param output_height  Desired output height in pixels
	/// @return Filtered texture ID, or input_texture if disabled/failed
	GLuint apply(GLuint input_texture, int output_width, int output_height);

	/// Calculate display dimensions accounting for PAR correction.
	void get_display_size(float base_width, float base_height, float scale, float &out_width, float &out_height) const;

	/// NTSC pixel aspect ratio: NES pixels are 8/7 wider than tall
	static constexpr float NTSC_PAR = 8.0f / 7.0f;

	// --- Settings (public for ImGui controls) ---
	bool enabled = false;			  ///< Master enable for CRT filter
	bool aspect_correction = true;	  ///< Apply 8:7 NTSC PAR correction
	float scanline_intensity = 0.25f; ///< Scanline darkness (0=none, 1=full)
	float curvature = 0.03f;		  ///< Barrel distortion strength (0=flat)
	float vignette_strength = 0.3f;	  ///< Edge darkening (0=none)
	float brightness = 1.15f;		  ///< Brightness boost (compensates for dimming)
	float mask_intensity = 0.06f;	  ///< Phosphor shadow mask strength

  private:
	bool load_gl_functions();
	bool create_shader_program();
	bool ensure_framebuffer(int width, int height);

	// GL resource IDs
	unsigned int shader_program_ = 0;
	unsigned int fbo_ = 0;
	unsigned int output_texture_ = 0;
	unsigned int vao_ = 0;
	unsigned int vbo_ = 0;

	// Cached uniform locations
	int loc_texture_ = -1;
	int loc_input_res_ = -1;
	int loc_output_res_ = -1;
	int loc_scanline_ = -1;
	int loc_curvature_ = -1;
	int loc_vignette_ = -1;
	int loc_brightness_ = -1;
	int loc_mask_ = -1;

	int fbo_width_ = 0;
	int fbo_height_ = 0;
	bool initialized_ = false;
	bool gl_loaded_ = false;
};

} // namespace nes::gui
