#include "gui/crt_filter.hpp"
#include <SDL3/SDL.h>
#include <cstdio>

// ─── OpenGL extension constants (not in Windows gl.h which is GL 1.1) ───────
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8889
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

// ─── GL extension function pointer types (loaded at runtime via SDL) ────────
// Shader
using PFN_glCreateShader = unsigned int(APIENTRY *)(unsigned int);
using PFN_glDeleteShader = void(APIENTRY *)(unsigned int);
using PFN_glShaderSource = void(APIENTRY *)(unsigned int, int, const char *const *, const int *);
using PFN_glCompileShader = void(APIENTRY *)(unsigned int);
using PFN_glGetShaderiv = void(APIENTRY *)(unsigned int, unsigned int, int *);
using PFN_glGetShaderInfoLog = void(APIENTRY *)(unsigned int, int, int *, char *);
// Program
using PFN_glCreateProgram = unsigned int(APIENTRY *)();
using PFN_glDeleteProgram = void(APIENTRY *)(unsigned int);
using PFN_glAttachShader = void(APIENTRY *)(unsigned int, unsigned int);
using PFN_glLinkProgram = void(APIENTRY *)(unsigned int);
using PFN_glGetProgramiv = void(APIENTRY *)(unsigned int, unsigned int, int *);
using PFN_glGetProgramInfoLog = void(APIENTRY *)(unsigned int, int, int *, char *);
using PFN_glUseProgram = void(APIENTRY *)(unsigned int);
using PFN_glBindAttribLocation = void(APIENTRY *)(unsigned int, unsigned int, const char *);
using PFN_glGetUniformLocation = int(APIENTRY *)(unsigned int, const char *);
using PFN_glUniform1i = void(APIENTRY *)(int, int);
using PFN_glUniform1f = void(APIENTRY *)(int, float);
using PFN_glUniform2f = void(APIENTRY *)(int, float, float);
// VAO
using PFN_glGenVertexArrays = void(APIENTRY *)(int, unsigned int *);
using PFN_glDeleteVertexArrays = void(APIENTRY *)(int, const unsigned int *);
using PFN_glBindVertexArray = void(APIENTRY *)(unsigned int);
// VBO
using PFN_glGenBuffers = void(APIENTRY *)(int, unsigned int *);
using PFN_glDeleteBuffers = void(APIENTRY *)(int, const unsigned int *);
using PFN_glBindBuffer = void(APIENTRY *)(unsigned int, unsigned int);
using PFN_glBufferData = void(APIENTRY *)(unsigned int, ptrdiff_t, const void *, unsigned int);
// Vertex attribs
using PFN_glVertexAttribPointer = void(APIENTRY *)(unsigned int, int, unsigned int, unsigned char, int, const void *);
using PFN_glEnableVertexAttribArray = void(APIENTRY *)(unsigned int);
// FBO
using PFN_glGenFramebuffers = void(APIENTRY *)(int, unsigned int *);
using PFN_glDeleteFramebuffers = void(APIENTRY *)(int, const unsigned int *);
using PFN_glBindFramebuffer = void(APIENTRY *)(unsigned int, unsigned int);
using PFN_glFramebufferTexture2D = void(APIENTRY *)(unsigned int, unsigned int, unsigned int, unsigned int, int);
using PFN_glCheckFramebufferStatus = unsigned int(APIENTRY *)(unsigned int);
// Misc
using PFN_glActiveTexture = void(APIENTRY *)(unsigned int);

// ─── GL function pointer instances ──────────────────────────────────────────
namespace {

#define CRT_GL_FUNC(name) static PFN_##name name##_ = nullptr

CRT_GL_FUNC(glCreateShader);
CRT_GL_FUNC(glDeleteShader);
CRT_GL_FUNC(glShaderSource);
CRT_GL_FUNC(glCompileShader);
CRT_GL_FUNC(glGetShaderiv);
CRT_GL_FUNC(glGetShaderInfoLog);
CRT_GL_FUNC(glCreateProgram);
CRT_GL_FUNC(glDeleteProgram);
CRT_GL_FUNC(glAttachShader);
CRT_GL_FUNC(glLinkProgram);
CRT_GL_FUNC(glGetProgramiv);
CRT_GL_FUNC(glGetProgramInfoLog);
CRT_GL_FUNC(glUseProgram);
CRT_GL_FUNC(glBindAttribLocation);
CRT_GL_FUNC(glGetUniformLocation);
CRT_GL_FUNC(glUniform1i);
CRT_GL_FUNC(glUniform1f);
CRT_GL_FUNC(glUniform2f);
CRT_GL_FUNC(glGenVertexArrays);
CRT_GL_FUNC(glDeleteVertexArrays);
CRT_GL_FUNC(glBindVertexArray);
CRT_GL_FUNC(glGenBuffers);
CRT_GL_FUNC(glDeleteBuffers);
CRT_GL_FUNC(glBindBuffer);
CRT_GL_FUNC(glBufferData);
CRT_GL_FUNC(glVertexAttribPointer);
CRT_GL_FUNC(glEnableVertexAttribArray);
CRT_GL_FUNC(glGenFramebuffers);
CRT_GL_FUNC(glDeleteFramebuffers);
CRT_GL_FUNC(glBindFramebuffer);
CRT_GL_FUNC(glFramebufferTexture2D);
CRT_GL_FUNC(glCheckFramebufferStatus);
CRT_GL_FUNC(glActiveTexture);

#undef CRT_GL_FUNC

// ─── GLSL 130 shader source code ───────────────────────────────────────────

const char *kVertexShader = R"glsl(
#version 130

in vec2 aPos;
in vec2 aTexCoord;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)glsl";

const char *kFragmentShader = R"glsl(
#version 130

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2 uInputRes;    // NES resolution (256, 240)
uniform vec2 uOutputRes;   // display pixel dimensions
uniform float uScanline;   // scanline intensity 0-1
uniform float uCurvature;  // barrel distortion amount 0-0.1
uniform float uVignette;   // edge darkening 0-1
uniform float uBrightness; // brightness multiplier
uniform float uMask;       // shadow mask intensity 0-1

// Barrel distortion simulating CRT screen curvature
vec2 barrel(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    uv *= 1.0 + uCurvature * r2;
    return uv * 0.5 + 0.5;
}

void main() {
    vec2 uv = vTexCoord;

    // Apply barrel distortion for CRT curvature
    if (uCurvature > 0.0) {
        uv = barrel(uv);
    }

    // Black outside the curved screen area
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Sample with bilinear filtering for natural CRT softness
    vec4 color = texture(uTexture, uv);

    // Scanlines: sin^2(y * 240 * pi) = 1 at scanline centers, 0 at gaps
    if (uScanline > 0.0) {
        float s = sin(uv.y * uInputRes.y * 3.14159265);
        s = s * s;
        color.rgb *= mix(1.0, s, uScanline);
    }

    // Shadow mask: subtle per-subpixel RGB phosphor pattern
    if (uMask > 0.0) {
        int subpx = int(mod(gl_FragCoord.x, 3.0));
        vec3 m = vec3(1.0);
        if (subpx == 0)      m = vec3(1.0, 1.0 - uMask, 1.0 - uMask);
        else if (subpx == 1) m = vec3(1.0 - uMask, 1.0, 1.0 - uMask);
        else                 m = vec3(1.0 - uMask, 1.0 - uMask, 1.0);
        color.rgb *= m;
    }

    // Vignette: darken edges like a real CRT
    if (uVignette > 0.0) {
        vec2 v = uv * (1.0 - uv);
        float vig = clamp(pow(v.x * v.y * 15.0, uVignette), 0.0, 1.0);
        color.rgb *= vig;
    }

    // Brightness boost compensating for scanline/vignette dimming
    color.rgb *= uBrightness;

    fragColor = color;
}
)glsl";

// Fullscreen quad vertex data: position (x,y) + texcoord (u,v)
// UV layout: GL bottom (NDC y=-1) -> V=0 -> first row of NES texture
// This matches ImGui::Image convention where UV(0,0) = top-left of display
// clang-format off
const float kQuadVerts[] = {
    // x      y       u     v
    -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
     1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
    -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
     1.0f,  1.0f,  1.0f, 1.0f,  // top-right
};
// clang-format on

} // anonymous namespace

namespace nes::gui {

CRTFilter::CRTFilter() = default;

CRTFilter::~CRTFilter() {
	shutdown();
}

bool CRTFilter::initialize() {
	if (initialized_)
		return true;

	if (!load_gl_functions()) {
		fprintf(stderr, "CRT filter: failed to load GL functions\n");
		return false;
	}

	if (!create_shader_program()) {
		fprintf(stderr, "CRT filter: failed to create shader program\n");
		return false;
	}

	// Create VAO + VBO for fullscreen quad
	glGenVertexArrays_(1, &vao_);
	glBindVertexArray_(vao_);

	glGenBuffers_(1, &vbo_);
	glBindBuffer_(GL_ARRAY_BUFFER, vbo_);
	glBufferData_(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

	// Attribute 0: position (vec2) at offset 0
	glVertexAttribPointer_(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
	glEnableVertexAttribArray_(0);

	// Attribute 1: texcoord (vec2) at offset 2 floats
	glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
						   reinterpret_cast<const void *>(2 * sizeof(float)));
	glEnableVertexAttribArray_(1);

	glBindVertexArray_(0);
	glBindBuffer_(GL_ARRAY_BUFFER, 0);

	initialized_ = true;
	fprintf(stderr, "CRT filter: initialized successfully\n");
	return true;
}

void CRTFilter::shutdown() {
	if (!initialized_)
		return;

	if (fbo_) {
		glDeleteFramebuffers_(1, &fbo_);
		fbo_ = 0;
	}
	if (output_texture_) {
		glDeleteTextures(1, &output_texture_);
		output_texture_ = 0;
	}
	if (vao_) {
		glDeleteVertexArrays_(1, &vao_);
		vao_ = 0;
	}
	if (vbo_) {
		glDeleteBuffers_(1, &vbo_);
		vbo_ = 0;
	}
	if (shader_program_) {
		glDeleteProgram_(shader_program_);
		shader_program_ = 0;
	}

	fbo_width_ = fbo_height_ = 0;
	initialized_ = false;
}

GLuint CRTFilter::apply(GLuint input_texture, int output_width, int output_height) {
	if (!enabled || !initialized_ || input_texture == 0 || output_width <= 0 || output_height <= 0) {
		return input_texture;
	}

	if (!ensure_framebuffer(output_width, output_height)) {
		return input_texture;
	}

	// Save current GL state so we don't break ImGui's rendering
	GLint prev_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	GLint prev_viewport[4];
	glGetIntegerv(GL_VIEWPORT, prev_viewport);
	GLint prev_texture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

	// Bind our FBO and set viewport to output dimensions
	glBindFramebuffer_(GL_FRAMEBUFFER, fbo_);
	glViewport(0, 0, output_width, output_height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Bind CRT shader and set uniforms
	glUseProgram_(shader_program_);

	glUniform1i_(loc_texture_, 0);
	glUniform2f_(loc_input_res_, 256.0f, 240.0f);
	glUniform2f_(loc_output_res_, static_cast<float>(output_width), static_cast<float>(output_height));
	glUniform1f_(loc_scanline_, scanline_intensity);
	glUniform1f_(loc_curvature_, curvature);
	glUniform1f_(loc_vignette_, vignette_strength);
	glUniform1f_(loc_brightness_, brightness);
	glUniform1f_(loc_mask_, mask_intensity);

	// Bind input texture with bilinear filtering for natural CRT softness
	glActiveTexture_(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, input_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Draw fullscreen quad through CRT shader
	glBindVertexArray_(vao_);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray_(0);

	// Restore input texture to nearest-neighbor filtering (ImGui/debug views expect it)
	glBindTexture(GL_TEXTURE_2D, input_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Restore previous GL state
	glUseProgram_(0);
	glBindFramebuffer_(GL_FRAMEBUFFER, static_cast<unsigned int>(prev_fbo));
	glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
	glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(prev_texture));

	return output_texture_;
}

void CRTFilter::get_display_size(float base_width, float base_height, float scale, float &out_width,
								 float &out_height) const {
	float par = (enabled && aspect_correction) ? NTSC_PAR : 1.0f;
	out_width = base_width * par * scale;
	out_height = base_height * scale;
}

bool CRTFilter::load_gl_functions() {
	if (gl_loaded_)
		return true;

// Load a GL extension function via SDL, bail on failure
#define LOAD_GL(name)                                                                                                  \
	name##_ = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress(#name));                                              \
	if (!name##_) {                                                                                                    \
		fprintf(stderr, "CRT filter: failed to load GL function: %s\n", #name);                                        \
		return false;                                                                                                  \
	}

	LOAD_GL(glCreateShader)
	LOAD_GL(glDeleteShader)
	LOAD_GL(glShaderSource)
	LOAD_GL(glCompileShader)
	LOAD_GL(glGetShaderiv)
	LOAD_GL(glGetShaderInfoLog)
	LOAD_GL(glCreateProgram)
	LOAD_GL(glDeleteProgram)
	LOAD_GL(glAttachShader)
	LOAD_GL(glLinkProgram)
	LOAD_GL(glGetProgramiv)
	LOAD_GL(glGetProgramInfoLog)
	LOAD_GL(glUseProgram)
	LOAD_GL(glBindAttribLocation)
	LOAD_GL(glGetUniformLocation)
	LOAD_GL(glUniform1i)
	LOAD_GL(glUniform1f)
	LOAD_GL(glUniform2f)
	LOAD_GL(glGenVertexArrays)
	LOAD_GL(glDeleteVertexArrays)
	LOAD_GL(glBindVertexArray)
	LOAD_GL(glGenBuffers)
	LOAD_GL(glDeleteBuffers)
	LOAD_GL(glBindBuffer)
	LOAD_GL(glBufferData)
	LOAD_GL(glVertexAttribPointer)
	LOAD_GL(glEnableVertexAttribArray)
	LOAD_GL(glGenFramebuffers)
	LOAD_GL(glDeleteFramebuffers)
	LOAD_GL(glBindFramebuffer)
	LOAD_GL(glFramebufferTexture2D)
	LOAD_GL(glCheckFramebufferStatus)
	LOAD_GL(glActiveTexture)

#undef LOAD_GL

	gl_loaded_ = true;
	return true;
}

bool CRTFilter::create_shader_program() {
	// Helper: compile a single shader stage
	auto compile_shader = [](unsigned int type, const char *source) -> unsigned int {
		unsigned int shader = glCreateShader_(type);
		glShaderSource_(shader, 1, &source, nullptr);
		glCompileShader_(shader);

		int success = 0;
		glGetShaderiv_(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char log[1024];
			glGetShaderInfoLog_(shader, sizeof(log), nullptr, log);
			fprintf(stderr, "CRT shader compile error: %s\n", log);
			glDeleteShader_(shader);
			return 0;
		}
		return shader;
	};

	unsigned int vs = compile_shader(GL_VERTEX_SHADER, kVertexShader);
	if (!vs)
		return false;

	unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
	if (!fs) {
		glDeleteShader_(vs);
		return false;
	}

	shader_program_ = glCreateProgram_();
	glAttachShader_(shader_program_, vs);
	glAttachShader_(shader_program_, fs);

	// Bind attribute locations before linking (GLSL 130 lacks layout qualifiers)
	glBindAttribLocation_(shader_program_, 0, "aPos");
	glBindAttribLocation_(shader_program_, 1, "aTexCoord");

	glLinkProgram_(shader_program_);

	// Shaders can be deleted after linking
	glDeleteShader_(vs);
	glDeleteShader_(fs);

	int success = 0;
	glGetProgramiv_(shader_program_, GL_LINK_STATUS, &success);
	if (!success) {
		char log[1024];
		glGetProgramInfoLog_(shader_program_, sizeof(log), nullptr, log);
		fprintf(stderr, "CRT shader link error: %s\n", log);
		glDeleteProgram_(shader_program_);
		shader_program_ = 0;
		return false;
	}

	// Cache uniform locations for fast access each frame
	loc_texture_ = glGetUniformLocation_(shader_program_, "uTexture");
	loc_input_res_ = glGetUniformLocation_(shader_program_, "uInputRes");
	loc_output_res_ = glGetUniformLocation_(shader_program_, "uOutputRes");
	loc_scanline_ = glGetUniformLocation_(shader_program_, "uScanline");
	loc_curvature_ = glGetUniformLocation_(shader_program_, "uCurvature");
	loc_vignette_ = glGetUniformLocation_(shader_program_, "uVignette");
	loc_brightness_ = glGetUniformLocation_(shader_program_, "uBrightness");
	loc_mask_ = glGetUniformLocation_(shader_program_, "uMask");

	return true;
}

bool CRTFilter::ensure_framebuffer(int width, int height) {
	if (fbo_ && fbo_width_ == width && fbo_height_ == height) {
		return true; // Already the right size
	}

	// Delete old resources if resizing
	if (fbo_) {
		glDeleteFramebuffers_(1, &fbo_);
		fbo_ = 0;
	}
	if (output_texture_) {
		glDeleteTextures(1, &output_texture_);
		output_texture_ = 0;
	}

	// Create output texture at the target resolution
	glGenTextures(1, &output_texture_);
	glBindTexture(GL_TEXTURE_2D, output_texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Create FBO and attach the output texture
	glGenFramebuffers_(1, &fbo_);
	glBindFramebuffer_(GL_FRAMEBUFFER, fbo_);
	glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_, 0);

	GLenum status = glCheckFramebufferStatus_(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "CRT filter: FBO incomplete (status 0x%x)\n", status);
		glBindFramebuffer_(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers_(1, &fbo_);
		fbo_ = 0;
		glDeleteTextures(1, &output_texture_);
		output_texture_ = 0;
		return false;
	}

	glBindFramebuffer_(GL_FRAMEBUFFER, 0);
	fbo_width_ = width;
	fbo_height_ = height;
	return true;
}

} // namespace nes::gui
