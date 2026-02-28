# VibeNES - Cycle-Accurate NES Emulator

You are helping develop a cycle-accurate NES emulator in C++23. You are an expert in NES hardware, 6502 CPU, PPU, APU, cartridge mappers, and cycle-accurate emulation techniques.

## Project Goals
- **Cycle-accurate emulation**: Cycle-by-cycle hardware behavior for perfect game compatibility
- **Modern C++23**: Concepts, ranges, `std::expected`, `constexpr`, strong types throughout
- **Clean architecture**: Modular CPU, PPU, APU, mapper, and bus components
- **Performance**: Real-time while maintaining accuracy

## Guidelines

### Communication
- Ask for clarification on unclear/contradictory requests before proceeding
- When in doubt, ask — better to clarify than implement incorrectly

### Documentation
- Do NOT create markdown reports/summaries unless explicitly requested
- Use inline code comments, not separate files
- After changes, provide a brief verbal summary

### Task Execution
- Always check task OUTPUT with `get_task_output`, not just task status
- Look for "Build Successful" / "Exit Code: 0" before proceeding
- Build tasks take time — verify completion in output before continuing

## Component Status (Accurate as of Feb 2026)

| Component | Status | Notes |
|-----------|--------|-------|
| CPU (6502) | ✅ Complete | 247 explicit opcodes + 9 catch-all NOPs (all 256 handled), hardware-accurate startup |
| PPU | ⚠️ Has bugs | Full rendering pipeline; P0 bugs #1-4 FIXED, increment_fine_y bug FIXED; P1/P2 bugs remain |
| APU | ✅ Substantial | All 5 channels (inline in header), frame counter, non-linear mixing, 1049 lines — NOT a stub |
| Bus/Memory | ✅ Complete | Full NES memory map, mirroring, open bus, dual-purpose registers |
| Mapper 0 (NROM) | ✅ Complete | 16KB/32KB PRG ROM |
| Mapper 1 (MMC1) | ✅ Fixed | Consecutive-write filter for RMW instructions added (was bug #9) |
| Mapper 2 (UxROM) | ✅ Complete | Includes bus conflict emulation |
| Mapper 3 (CNROM) | ✅ Complete | CHR ROM bank switching, bus conflict emulation, 16KB/32KB PRG |
| Mapper 4 (MMC3) | ⚠️ Has bugs | Banking works, IRQ counter uses fixed cycle 260 instead of A12 tracking |
| Cartridge/ROM | ✅ Complete | iNES loading, mapper factory, GUI file browser |
| Save States | ✅ Complete | Serialize/deserialize with CRC32 verification, 316 lines |
| Audio Backend | ✅ Complete | SDL2 audio output + sample rate converter |
| Input | ✅ Complete | Controller + gamepad manager |
| GUI | ✅ Complete | SDL2 + ImGui, 7 panels (CPU, disassembler, memory, ROM, PPU, timing, audio), retro theme |
| Disassembler | ✅ Complete | All 256 opcodes with addressing modes |
| Tests | ⚠️ Partial | 3 CPU + 18 PPU + 2 core + 1 memory test files (186 tests, all passing). Zero tests for APU, mappers, save states, cartridge, input |

## Known Bugs (Priority Order)

### FIXED (Phase 1 — completed Feb 2026)
- ~~Bug #1: `set_vblank_flag()` called every VBlank dot~~ — Fixed: only clears sprite0/overflow at pre-render dot 1
- ~~Bug #2: Palette PPUDATA reads skip VRAM increment~~ — Fixed: always increments after $2007 reads
- ~~Bug #3: Extra `tick_single_dot()` on PPUDATA access~~ — Fixed: removed phantom PPU dot calls
- ~~Bug #4: Palette $3F00 write propagates to $04/$08/$0C~~ — Fixed: correct mirror behavior
- ~~Bug #6: Bus immediately clears mapper IRQ~~ — Fixed: IRQ stays asserted until acknowledged
- ~~Bug #9: MMC1 missing consecutive-write filter~~ — Fixed: RMW instructions handled correctly
- ~~Bug #13: `handle_rendering_disable_mid_scanline` static local~~ — Fixed: converted to member variable
- ~~Bug #15: `increment_fine_y()` dead code in `process_visible_scanline()`~~ — Fixed: if/else-if boundary at cycle 256 made increment_fine_y unreachable; moved into first block. This was causing only sprite 0 to render (background row never advanced, hiding behind-BG sprites).

### P1 — Synchronization and Mapper Issues (Remaining)
5. **Instruction-level sync, not cycle-level** — CPU runs entire instructions atomically, then bulk-ticks PPU. Mid-instruction PPU state changes invisible. `consume_cycle()` only decrements a counter, doesn't synchronize.
7. **OAM DMA doesn't halt CPU** — Main loop calls `execute_instruction()` directly, bypassing `tick()` which has DMA check. CPU keeps running during DMA.
8. **MMC3 A12 IRQ at fixed cycle 260** — `ppu.cpp` hardcodes toggle instead of tracking actual A12 transitions. `track_a12_line()` exists but is never called.

### P2 — Moderate Issues (Remaining)
10. **APU uses edge-triggered IRQ** — `apu.cpp` uses edge detection; NES APU IRQ is level-triggered.
11. **DMC DMA cycle stealing not implemented** — Fields exist but never activated.
12. ~~**`step_frame()` only runs 1000 cycles**~~ — Fixed: `gui_application.cpp` now runs 29,781 cycles per frame.
14. **Frame 0 hack** — `ppu.cpp` clears fine_y/fine_x on frame 0 as a workaround for init timing.

## Architecture Notes

### Current Synchronization Model
The CPU executes an entire instruction via `execute_instruction()`, returns consumed cycle count, then `bus_->tick(N)` bulk-advances PPU (N×3 dots), APU (N cycles), etc. `consume_cycle()` inside instructions only decrements `cycles_remaining_` — no PPU/APU interleaving occurs mid-instruction.

### Mapper Factory
`MapperFactory::create_mapper()` dispatches by mapper number. Mapper base class defines CPU/PPU memory access, mirroring control, and reset. Bus conflict emulation in Mapper 2.

## Roadmap

### Phase 1: Fix Critical PPU Bugs
✅ **DONE** — Fixed bugs #1-4 (VBlank flag spam, palette increment, phantom ticks, palette mirroring), #6 (mapper IRQ clearing), #9 (MMC1 consecutive writes), #13 (static local).

### Phase 2: Remove Dead Code and Debris
✅ **DONE** — Deleted: `debug_vblank.cpp`, `ideas.txt`, all stub headers (`apu_stub.hpp`, `ppu_stub.hpp`, `cartridge_stub.hpp`, `controller_stub.hpp`), old GUI headers (`emulator_gui.hpp`, `memory_viewer.hpp`, `ppu_viewer.hpp`), empty files (`mapper.cpp`, `rom_analyzer.cpp`, `palette_generator.cpp`, `docs/development_notes.md`), empty dirs (`src/memory/`, `src/debug/`, `include/debug/`, `tools/`, `include/apu/channels/`, `src/apu/channels/`). Trimmed `third_party/imgui/` to core + SDL2/OpenGL3 backends only (removed `examples/`, `docs/`, `misc/`, `.github/`, `imgui_demo.cpp`, all unused backends). Deleted `build_scripts/` (had MSYS2 g++ hardcoded paths). Migrated Catch2 from vendored amalgamated (`tests/catch2/`) to vcpkg; deleted `tests/catch2/` and `tests/test_main.cpp`.

### Phase 3: Migrate to CMake
✅ **DONE** — Migrated from MSYS2/GCC PowerShell scripts to MSVC + CMake + vcpkg + Ninja. All MSYS2/GCC references purged from the entire project. CMakeLists.txt defines `vibes_core`, `VibeNES_GUI`, `VibeNES_Tests`, and `imgui` targets. CMakePresets.json provides debug/release presets. Both `VibeNES_GUI.exe` and `VibeNES_Tests.exe` build successfully. `.vscode/c_cpp_properties.json` deleted (CMake Tools provides IntelliSense).

### Phase 4: Cycle-Level CPU/PPU/APU Interleaving
**Approach: "Fat `consume_cycle()`"** — The ~188 instruction methods already call `consume_cycle()` / `read_byte()` / `write_byte()` at correct cycle boundaries. Change `consume_cycle()` from counter decrement to `bus_->tick_single_cpu_cycle()` (advances PPU 3 dots + APU 1 cycle + checks IRQ). This makes the entire system cycle-accurate without rewriting instruction logic. Then: implement proper OAM DMA halt, cycle-accurate interrupt polling (penultimate cycle), wire MMC3 A12 tracking into fetch pipeline (fix #8), implement DMC cycle stealing (fix #11), remove frame 0 hack (fix #14).

### Phase 5: Documentation and Test Coverage
Update README.md to match reality. Add tests for APU, mappers, save states. Add mapper/APU test ROM infrastructure.

## NES Hardware Reference

### Specifications
- **CPU**: 6502 at ~1.789773 MHz (NTSC), 1 CPU cycle = 3 PPU dots
- **PPU**: 341 dots/scanline, 262 scanlines/frame, 256×240 visible
- **Memory**: 2KB work RAM (mirrored ×4), 2KB VRAM, 256B OAM
- **Palette**: 64 colors, 25 usable simultaneously

### Memory Maps
```
CPU: $0000-$07FF RAM | $0800-$1FFF mirrors | $2000-$2007 PPU regs (mirrored to $3FFF)
     $4000-$4017 APU/IO | $4020-$FFFF cartridge
PPU: $0000-$1FFF pattern tables | $2000-$2FFF nametables | $3F00-$3F1F palette RAM
```

## Coding Standards
- C++23: `std::expected`, concepts, ranges, `constexpr`/`consteval`, strong enums, `std::span`, `std::bit_cast`
- RAII, no raw pointers, const-correctness throughout
- Accuracy over performance; optimize only after correctness is proven
- Cycle timing comments on all hardware operations

## CPU Implementation Patterns
- Manual PC management: `read_byte()` + explicit PC increment, never `read_word()`
- Page crossing: `(base & 0xFF00) != ((base + offset) & 0xFF00)`
- `consume_cycle()` for each hardware cycle; cycle 1 is opcode fetch in `execute_instruction()`

## Testing Patterns
- Use RAM addresses 0x0500-0x07FF for test data (avoids zero page, stack, PPU/APU bus conflicts)
- Exact cycle counts in assertions
- Separate instruction and data addresses
- If tests fail with "unknown opcode" — check memory address ranges first, not instruction logic
- All 186 tests pass. Catch2 v3 via vcpkg with `catch_discover_tests()` for per-TEST_CASE CTest entries

### Test Example
```cpp
TEST_CASE("LDA Absolute,X", "[cpu][instructions][timing]") {
    auto bus = std::make_unique<SystemBus>();
    auto ram = std::make_shared<Ram>();
    bus->connect_ram(ram);
    CPU6502 cpu(bus.get());

    SECTION("No page cross (4 cycles)") {
        cpu.set_program_counter(0x0100);
        cpu.set_x_register(0x10);
        bus->write(0x0210, 0x42);
        bus->write(0x0100, 0xBD); // LDA abs,X
        bus->write(0x0101, 0x00);
        bus->write(0x0102, 0x02);
        cpu.tick(cpu_cycles(4));
        REQUIRE(cpu.get_accumulator() == 0x42);
        REQUIRE(cpu.get_program_counter() == 0x0103);
    }
}
```

## Build System
MSVC v143 (Build Tools 2022 v17.14.27) + CMake 3.31.6 + Ninja 1.12.1 + vcpkg (project-local). Dependencies: SDL2 2.32.10 (vcpkg, x64-windows), Catch2 3.13.0 (vcpkg, x64-windows), ImGui 1.91.6 (third_party/imgui/, compiled as static lib — vcpkg imgui lacks sdl2-binding), OpenGL3. MSYS2 fully uninstalled from machine.

### Key Paths
- **Build Tools**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- **CMake**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- **Ninja**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- **vcvarsall**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`
- **vcpkg**: `./vcpkg/vcpkg.exe` (project-local)
- **Build output**: `build/debug/` and `build/release/`

### Build Commands
Use VS Code tasks (Ctrl+Shift+B for default build). All tasks use `cmd.exe`, source `vcvarsall.bat`, and use explicit MSVC cmake/ninja paths via env vars. No dependency on CMake Tools extension for building.
- **Build Debug** (default build task)
- **Build Release**
- **Run Tests** (builds debug first, then runs ctest)
- **Run VibeNES** (builds debug first, then launches GUI)
- **Clean** (deletes build/debug and build/release)

CMakePresets.json pins `CMAKE_MAKE_PROGRAM` to MSVC's Ninja to prevent stale PATH issues.

### CMake Targets
- `imgui` — static lib from `third_party/imgui/` (core + SDL2/OpenGL3 backends)
- `vibes_core` — static lib with all emulation code (CPU, PPU, APU, Bus, Cartridge, Input, SaveState)
- `VibeNES_GUI` — main executable (links vibes_core, imgui, opengl32)
- `VibeNES_Tests` — test executable (links vibes_core + Catch2::Catch2WithMain from vcpkg, `catch_discover_tests()` for per-test CTest)

### MSVC-Specific Fixes Applied
- `src/main.cpp` has `#define SDL_MAIN_HANDLED` at line 1 (prevents SDL2 main hijack)
- `include/cartridge/rom_loader.hpp` has explicit `#include <array>` (MSVC doesn't transitively include it)
- `include/gui/panels/ppu_viewer_panel.hpp`, `src/gui/gui_application.cpp`, `src/gui/panels/ppu_viewer_panel.cpp` all have `#define NOMINMAX` + `#define WIN32_LEAN_AND_MEAN` + `#include <windows.h>` before `#include <GL/gl.h>`
- `src/ppu/nes_palette.cpp` — removed `#ifdef NES_GUI_ENABLED` guard; always compiles `get_imgui_color()` since `vibes_core` has imgui include path
- `ImGui::ShowDemoWindow()` call commented out in `gui_application.cpp` (we removed `imgui_demo.cpp`)
- `CMakeLists.txt` uses link options for subsystem (CONSOLE debug, WINDOWS release) instead of `WIN32` on `add_executable`
- All targets use generator expressions for debug/release compile options (no D9025 override warnings)

### Build Warnings
✅ **Zero warnings** — All C4244 narrowing warnings fixed with explicit `static_cast<int>()` in cpu_6502.cpp (3 sites where `CpuCycle::count()` returns `int64_t`).

### VS Code Integration
- `.vscode/tasks.json` — Shell tasks via `cmd.exe` with explicit MSVC paths (env vars `%VCVARS%`, `%CMAKE%`). No CMake Tools extension dependency for build/test.
- `.vscode/settings.json` — CMake Tools for IntelliSense only (`configurationProvider: "ms-vscode.cmake-tools"`). Includes "MSVC Developer CMD" terminal profile.
- `.vscode/launch.json` — Two `cppvsdbg` configurations: "Debug VibeNES" and "Debug Tests", both use `preLaunchTask: "Build Debug"`.
- No `c_cpp_properties.json` — CMake Tools provides IntelliSense via `compile_commands.json`.

## Current State (as of Feb 28, 2026)
- **Phases 1-3 complete**. Debug and release builds green, **zero warnings**. MSYS2 fully removed from machine.
- **All 186 tests pass** (0 failures). Catch2 v3 via vcpkg with `catch_discover_tests()`. Previous 14 test failures fixed (interrupt handling, palette mirroring, fine_x scroll, pattern table ctrl bits, OAM DMA, timing, sprite overflow).
- **Games tested**: Super Mario Bros. (NROM/Mapper 0) — background + all sprites render correctly. Crystalis (MMC1/Mapper 1) — boots and runs.
- **Dependencies**: SDL2 + Catch2 via vcpkg. ImGui vendored (vcpkg port lacks sdl2-binding feature).
- **Next step**: Proceed to Phase 4 (cycle-level interleaving).
