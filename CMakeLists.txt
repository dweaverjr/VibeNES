cmake_minimum_required(VERSION 3.25)
project(VibeNES VERSION 0.1.0 LANGUAGES CXX)

# ─── C++23 Standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Find SDL2 via vcpkg ─────────────────────────────────────────────────────
find_package(SDL2 CONFIG REQUIRED)

# ─── ImGui static library ────────────────────────────────────────────────────
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC SDL2::SDL2)
# Suppress warnings in third-party code
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# ─── Core emulation library ──────────────────────────────────────────────────
add_library(vibes_core STATIC
    # CPU
    src/cpu/cpu_6502.cpp
    # PPU
    src/ppu/ppu.cpp
    src/ppu/ppu_memory.cpp
    src/ppu/nes_palette.cpp
    # APU
    src/apu/apu.cpp
    # Audio
    src/audio/audio_backend.cpp
    src/audio/sample_rate_converter.cpp
    # Core
    src/core/bus.cpp
    # Cartridge & Mappers
    src/cartridge/cartridge.cpp
    src/cartridge/mapper_factory.cpp
    src/cartridge/rom_loader.cpp
    src/cartridge/mappers/mapper_000.cpp
    src/cartridge/mappers/mapper_001.cpp
    src/cartridge/mappers/mapper_002.cpp
    src/cartridge/mappers/mapper_003.cpp
    src/cartridge/mappers/mapper_004.cpp
    # Input
    src/input/controller.cpp
    src/input/gamepad_manager.cpp
    # System
    src/system/save_state.cpp
)
target_include_directories(vibes_core PUBLIC include)
target_link_libraries(vibes_core PUBLIC SDL2::SDL2)
# ImGui headers needed for NESPalette::get_imgui_color (ImVec4)
target_include_directories(vibes_core PRIVATE third_party/imgui)
if(MSVC)
    target_compile_options(vibes_core PRIVATE
        /W4 /permissive- /Zc:__cplusplus
        $<$<CONFIG:Debug>:/Zi /Od /RTC1>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
else()
    target_compile_options(vibes_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g3 -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
endif()

# ─── GUI executable ──────────────────────────────────────────────────────────
add_executable(VibeNES_GUI
    src/main.cpp
    src/gui/gui_application.cpp
    src/gui/panels/audio_panel.cpp
    src/gui/panels/cpu_state_panel.cpp
    src/gui/panels/disassembler_panel.cpp
    src/gui/panels/memory_viewer_panel.cpp
    src/gui/panels/ppu_viewer_panel.cpp
    src/gui/panels/rom_loader_panel.cpp
    src/gui/panels/timing_panel.cpp
    src/gui/style/retro_theme.cpp
)
target_compile_definitions(VibeNES_GUI PRIVATE NES_GUI_ENABLED)
target_link_libraries(VibeNES_GUI PRIVATE vibes_core imgui opengl32)
if(MSVC)
    target_compile_options(VibeNES_GUI PRIVATE
        /W4 /permissive- /Zc:__cplusplus
        $<$<CONFIG:Debug>:/Zi /Od /RTC1>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
    # Show console in Debug for printf debugging, hide in Release
    target_link_options(VibeNES_GUI PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
else()
    target_compile_options(VibeNES_GUI PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g3 -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
    target_link_options(VibeNES_GUI PRIVATE
        $<$<CONFIG:Release>:-mwindows>
    )
endif()

# Copy SDL2.dll next to executable after build
add_custom_command(TARGET VibeNES_GUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:VibeNES_GUI>
    COMMENT "Copying SDL2.dll to output directory"
)

# ─── Tests executable ────────────────────────────────────────────────────────
file(GLOB_RECURSE TEST_SOURCES
    tests/core/*.cpp
    tests/cpu/*.cpp
    tests/memory/*.cpp
    tests/ppu/*.cpp
)
# Exclude unity-build files that #include other .cpp files
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_ppu_main\\.cpp$")

add_executable(VibeNES_Tests
    tests/test_main.cpp
    tests/catch2/catch_amalgamated.cpp
    ${TEST_SOURCES}
)
target_include_directories(VibeNES_Tests PRIVATE tests)
target_link_libraries(VibeNES_Tests PRIVATE vibes_core)
if(MSVC)
    target_compile_options(VibeNES_Tests PRIVATE
        /W3 /permissive- /Zc:__cplusplus
        /bigobj
        $<$<CONFIG:Debug>:/Zi /Od /RTC1>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
else()
    target_compile_options(VibeNES_Tests PRIVATE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g3 -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
endif()

# Register with CTest
enable_testing()
add_test(NAME AllTests COMMAND VibeNES_Tests)
