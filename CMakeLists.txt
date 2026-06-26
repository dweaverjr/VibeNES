cmake_minimum_required(VERSION 3.25)
project(VibeNES VERSION 1.0.0 LANGUAGES CXX)

# ─── C++23 Standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)

check_ipo_supported(RESULT VIBENES_IPO_SUPPORTED OUTPUT VIBENES_IPO_ERROR)
if(VIBENES_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(WARNING "Release IPO/LTO disabled: ${VIBENES_IPO_ERROR}")
endif()

# Enable exception handling globally (MSVC needs this explicitly)
if(MSVC)
    add_compile_options(/EHsc)
    add_compile_options($<$<CONFIG:Release>:/Gw>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
else()
    add_compile_options($<$<CONFIG:Release>:-ffunction-sections -fdata-sections>)
    add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
endif()

# ─── Find SDL3 and ImGui via vcpkg ───────────────────────────────────────────
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

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
    src/system/battery_save.cpp
)
target_include_directories(vibes_core PUBLIC include)
target_link_libraries(vibes_core PUBLIC SDL3::SDL3)
# ImGui headers needed for NESPalette::get_imgui_color (ImVec4)
target_link_libraries(vibes_core PRIVATE imgui::imgui)
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
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# ─── GUI executable ──────────────────────────────────────────────────────────
add_executable(VibeNES_GUI
    src/main.cpp
    src/gui/gui_application.cpp
    src/gui/crt_filter.cpp
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
target_link_libraries(VibeNES_GUI PRIVATE vibes_core imgui::imgui opengl32)
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
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
    target_link_options(VibeNES_GUI PRIVATE
        $<$<CONFIG:Release>:-mwindows>
    )
endif()

# Copy SDL3.dll next to executable after build
add_custom_command(TARGET VibeNES_GUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:VibeNES_GUI>
    COMMENT "Copying SDL3.dll to output directory"
)

# ─── Tests executable ────────────────────────────────────────────────────────
find_package(Catch2 3 REQUIRED)

file(GLOB_RECURSE TEST_SOURCES
    tests/apu/*.cpp
    tests/cartridge/*.cpp
    tests/core/*.cpp
    tests/cpu/*.cpp
    tests/memory/*.cpp
    tests/ppu/*.cpp
)
# Exclude unity-build files that #include other .cpp files
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_ppu_main\\.cpp$")

add_executable(VibeNES_Tests ${TEST_SOURCES})
target_include_directories(VibeNES_Tests PRIVATE tests)
target_link_libraries(VibeNES_Tests PRIVATE vibes_core Catch2::Catch2WithMain)
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
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# ─── CTest ────────────────────────────────────────────────────────────────────
# catch_discover_tests registers each TEST_CASE as its own CTest entry.
include(CTest)
include(Catch)
catch_discover_tests(VibeNES_Tests)

# ─── Install rules ────────────────────────────────────────────────────────────
# cmake --install build/release --prefix <dest>  puts everything in <dest>.
# CPack uses these rules to build the installer.

install(TARGETS VibeNES_GUI
    RUNTIME DESTINATION .   # exe goes in install root
)

# SDL3.dll alongside the exe
install(FILES $<TARGET_FILE:SDL3::SDL3>
    DESTINATION .
)

# README + license in the install root for end users.
install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION .
)

# Package ROM library and save-state data for portable migration.
# This preserves current working setup when moving to another machine/location.
install(DIRECTORY roms/
    DESTINATION roms
)

# Ship an empty saves/ directory. The legacy saves/roms/ layout is intentionally
# excluded: the app now writes slots directly to saves/, so bundling the old
# saves/roms/ copies would scatter saves across two locations.
install(DIRECTORY saves/
    DESTINATION saves
    PATTERN "roms" EXCLUDE
)

# ─── CPack (Windows ZIP + NSIS installer) ────────────────────────────────────
set(CPACK_PACKAGE_NAME              "VibeNES")
set(CPACK_PACKAGE_VENDOR            "VibeNES")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cycle-accurate NES emulator")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VibeNES")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_SOURCE_DIR}/LICENSE")

# Output filename: VibeNES-1.0.0-win64
set(CPACK_PACKAGE_FILE_NAME         "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-win64")

# ── ZIP: simple portable archive ────────────────────────────────────────────
set(CPACK_GENERATOR "ZIP")

# ── NSIS: Add NSIS to the list only when makensis is available ───────────────
find_program(MAKENSIS_EXECUTABLE makensis)
if(MAKENSIS_EXECUTABLE)
    list(APPEND CPACK_GENERATOR "NSIS")

    set(CPACK_NSIS_DISPLAY_NAME         "VibeNES")
    set(CPACK_NSIS_PACKAGE_NAME         "VibeNES")
    set(CPACK_NSIS_INSTALL_ROOT         "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH          OFF)

    # Desktop + Start Menu shortcuts pointing at VibeNES_GUI.exe in install root
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortcut '$DESKTOP\\\\VibeNES.lnk' '$INSTDIR\\\\VibeNES_GUI.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\VibeNES.lnk'"
    )

    # After install: open the roms folder so the user knows where to put ROMs
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "ExecShell 'open' '$INSTDIR\\\\roms'"
    )
endif()

include(CPack)
