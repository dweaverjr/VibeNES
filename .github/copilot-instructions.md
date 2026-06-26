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

## Component Status (Accurate as of May 2026)

| Component | Status | Notes |
|-----------|--------|-------|
| CPU (6502) | ✅ Complete | 247 explicit opcodes + 9 catch-all NOPs (all 256 handled), hardware-accurate startup |
| PPU | ✅ Substantial | Full rendering pipeline; P0 bugs #1-4 FIXED, increment_fine_y bug FIXED, per-cycle sprite fetches (257-320). Minor edge-case rendering quirks remain |
| APU | ✅ Complete | All 5 channels (inline in header), frame counter, non-linear mixing, level-triggered IRQ (bug #10 fixed) |
| Bus/Memory | ✅ Complete | Full NES memory map, mirroring, open bus, dual-purpose registers |
| Mapper 0 (NROM) | ✅ Complete | 16KB/32KB PRG ROM |
| Mapper 1 (MMC1) | ✅ Complete | Consecutive-write filter for RMW instructions (was bug #9) |
| Mapper 2 (UxROM) | ✅ Complete | Bus conflict emulation (ANDs against ROM byte at written address) |
| Mapper 3 (CNROM) | ✅ Complete | CHR ROM bank switching, bus conflict emulation, 16KB/32KB PRG |
| Mapper 4 (MMC3) | ✅ Complete | Banking, per-cycle sprite pattern fetches (257-320), A12 low-time filter, proper IRQ line deassertion. Crystalis playable. |
| Cartridge/ROM | ✅ Complete | iNES loading, mapper factory, GUI file browser |
| Save States | ✅ Complete | Serialize/deserialize with CRC32 verification, bounds-checked read helpers (throw on truncated/malicious files) |
| Audio Backend | ✅ Complete | SDL3 audio output + sample rate converter |
| Input | ✅ Complete | Controller + gamepad manager |
| GUI | ✅ Complete | SDL3 + ImGui (vcpkg, sdl3-binding), 7 panels (CPU, disassembler, memory, ROM, PPU, timing, audio), retro theme |
| Disassembler | ✅ Complete | All 256 opcodes with addressing modes |
| Tests | ✅ Substantial | 242 tests, all passing. Cover CPU, PPU, APU, all 5 mappers, ROM loader, save states. No tests for input. |

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
5. ~~**Instruction-level sync, not cycle-level**~~ — **FIXED (Phase 4)**: `consume_cycle()` now calls `bus_->tick_single_cpu_cycle()` which advances PPU 3 dots + APU 1 cycle + checks mapper IRQs. Every `read_byte()`/`write_byte()`/`consume_cycle()` call interleaves correctly. Main loop no longer calls `bus_->tick()` post-instruction. Interrupt handler cycle counts fixed (were double-counting memory operation cycles).
7. ~~**OAM DMA doesn't halt CPU**~~ — **FIXED (Phase 4)**: DMA moved from PPU-driven (per PPU dot, 3× too fast) to CPU-driven. `execute_instruction()` checks `bus_->is_oam_dma_pending()` and calls `execute_oam_dma()`, which burns 513 CPU cycles (1 dummy + 256×(read+write)) via `consume_cycle()`, properly interleaving PPU/APU. CPU PC does not advance during DMA.
8. ~~**MMC3 A12 IRQ at fixed cycle 260**~~ — **FIXED (Phase 4+5)**: Replaced once-per-scanline `a12_clocked_this_scanline_` clamp with a proper A12 low-time filter. PPU tracks monotonic dot counter (`ppu_dot_counter_`) and clocks mapper IRQ counter on rising A12 edges when A12 has been low for ≥15 PPU cycles. Phase 5 added per-cycle sprite pattern fetches (cycles 257-320, 8 cycles/sprite: garbage NT, garbage attr, pattern low, pattern high) so A12 transitions match real hardware. Removed non-standard `irq_initialized_` guard from Mapper004. Bus IRQ line management fixed to deassert when no sources active.

### P2 — Moderate Issues (Remaining)
10. ~~**APU uses edge-triggered IRQ**~~ — **FIXED**: `apu.cpp` now drives a level-triggered IRQ line. `irq_line_asserted_ = (frame_irq_flag_ || dmc_irq_flag_)` is recomputed each tick; the bus reflects the level rather than a one-shot edge. Regression test added.
11. ~~**DMC DMA cycle stealing not implemented**~~ — **FIXED (Phase 4)**: `load_sample_byte()` replaced with a signal-based DMA request.  When the DMC sample buffer empties, `APU::tick()` sets `dmc_dma_pending_` + `dmc_dma_address_`.  CPU’s `consume_cycle()` detects the pending DMA, stalls for ~4 cycles (3 dummy + 1 read) while continuing PPU/APU interleaving, then calls `bus_->service_dmc_dma()` which reads the byte and delivers it via `APU::complete_dmc_dma()`.
12. ~~**`step_frame()` only runs 1000 cycles**~~ — Fixed: `gui_application.cpp` now runs 29,781 cycles per frame.
14. ~~**Frame 0 hack**~~ — **FIXED (Phase 4)**: Removed workaround that cleared fine_x/fine_y on frame 0.  PPU now honours whatever scroll/VRAM state software has configured from the very first frame.

### Remaining latent issues (from code review 2026-05-29)
- **Minor PPU edge-case rendering quirks**: some obscure mid-scanline register-write corner cases remain.
- Save-state read helpers are now bounds-checked (throw on overrun) and the PPU sprite-attribute unpack uses `std::bit_cast` (no strict-aliasing UB).

## Architecture Notes

### Current Synchronization Model
Cycle-level interleaving via “fat `consume_cycle()`”. Each `consume_cycle()` call inside CPU instructions calls `bus_->tick_single_cpu_cycle()`, which advances PPU by 3 dots, APU by 1 cycle, and checks mapper IRQs.  Additionally, each `consume_cycle()` performs **penultimate-cycle interrupt polling**: it shifts current NMI/IRQ samples into `prev_*` and re-samples `curr_*`, so `prev_nmi_pending_`/`prev_irq_signal_` hold the penultimate-cycle state at instruction boundaries.  `consume_cycle()` also checks for pending **DMC DMA** requests from the APU and stalls ~4 CPU cycles to perform the sample byte read.  The main loop (`gui_application.cpp`) calls `execute_instruction()` which returns the consumed cycle count (tracked via `cycles_consumed_` counter); no separate `bus_->tick()` is needed. `cycles_remaining_` is still decremented for the `CPU6502::tick()` budget loop used by tests.

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
✅ **DONE** — Fat `consume_cycle()` implemented (bug #5 fixed). Each `consume_cycle()` now calls `bus_->tick_single_cpu_cycle()`, providing per-cycle CPU/PPU/APU interleaving. Interrupt handler cycle counts corrected (NMI/IRQ: 2 internal + 5 memory ops = 7 total; RESET: 5 internal/suppressed + 2 reads = 7 total). Main loop `bus_->tick()` calls removed. All 189 tests pass.
OAM DMA halt implemented (bug #7 fixed). DMA moved from PPU-driven to CPU-driven: `execute_oam_dma()` burns 513 cycles (1 dummy + 256×(read+write)) via `consume_cycle()`, properly interleaving PPU/APU. CPU PC does not advance during DMA.
MMC3 A12 IRQ fixed (bug #8 fixed). Replaced once-per-scanline clamp with A12 low-time filter (≥15 PPU dots). Monotonic `ppu_dot_counter_` tracks time since A12 was last high. Phase 5 further fixed: per-cycle sprite pattern fetches (257-320), removed `irq_initialized_` guard, bus IRQ deassertion.
Penultimate-cycle interrupt polling implemented. `consume_cycle()` samples NMI/IRQ lines each cycle; `execute_instruction()` checks `prev_nmi_pending_`/`prev_irq_signal_` (the penultimate-cycle sample) at instruction boundaries. This correctly models CLI delay (IRQ not taken until instruction after CLI) and SEI window (one IRQ sneaks through after SEI). 3 new timing tests added.
DMC DMA cycle stealing implemented (bug #11 fixed). APU signals `dmc_dma_pending_` when sample buffer empties; CPU’s `consume_cycle()` stalls ~4 cycles and performs the DMA read via `bus_->service_dmc_dma()` → `APU::complete_dmc_dma()`.
Frame 0 hack removed (bug #14 fixed). PPU honours initial scroll/VRAM state from the first frame.

### Phase 5: Documentation and Test Coverage
✅ **SUBSTANTIALLY DONE** — README.md rewritten and refreshed to match current project reality (SDL3, correct dependency versions, accurate Known Issues). Added 53 new tests (189→242): APU tests (all 5 channels, frame counter, registers, mixing, serialization, DMA), mapper tests (all 5 mappers: banking, IRQ, bus conflicts, serialization), ROM loader tests (iNES parsing, header validation), save state tests (header magic, version, validation). Code-review fixes applied: bounds-checked save-state read helpers, `std::bit_cast` for sprite attributes. Remaining: input tests, test ROM infrastructure.

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
- All 242 tests pass. Catch2 v3 via vcpkg with `catch_discover_tests()` for per-TEST_CASE CTest entries

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
MSVC v143 (Build Tools 2022) + CMake + Ninja + vcpkg (project-local). Dependencies (all via vcpkg, x64-windows): SDL3 3.4.8, Catch2 3.15.0, ImGui 1.92.8 (with `sdl3-binding` + `opengl3-binding` features), OpenGL3. MSYS2 fully uninstalled from machine.

### Key Paths
- **Build Tools**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- **CMake**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- **Ninja**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- **vcvarsall**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`
- **vcpkg**: `./vcpkg/vcpkg.exe` (project-local)
- **Build output**: `build/debug/` and `build/release/`

### Build Commands
**ALWAYS build and test via the VS Code tasks (the `run_task` tool), NEVER by typing build commands into a terminal.** The tasks reference env vars like `%VCVARS%` and `%CMAKE%` that are ONLY defined inside the `cmd.exe` task shell — they do NOT exist in the integrated PowerShell terminal, so `run_in_terminal` calls like `%VCVARS% && %CMAKE% --build ...` will fail with "term not recognized". Do not try to reconstruct the raw cmake/ninja/vcvarsall command line by hand.

Workflow for any build/test/run:
1. Call `run_task` with the appropriate task id (e.g. `shell: Build Debug`).
2. After it finishes, check the task OUTPUT with `get_task_output` and confirm `Exit Code: 0` / "Build Successful" before proceeding — do not rely on status alone.

Available tasks (use these exact labels):
- **Build Debug** (default build task — `shell: Build Debug`)
- **Build Release**
- **Run Tests** (builds debug first, then runs ctest)
- **Run Tests: CPU** / **Run Tests: PPU** / **Run Tests: by name...**
- **Run VibeNES (Debug)** / **Run VibeNES (Release)** (builds first, then launches GUI)
- **Clean** (deletes build/debug and build/release)

CMakePresets.json pins `CMAKE_MAKE_PROGRAM` to MSVC's Ninja to prevent stale PATH issues.

### CMake Targets
- `vibes_core` — static lib with all emulation code (CPU, PPU, APU, Bus, Cartridge, Input, SaveState). Links `SDL3::SDL3` (public) and `imgui::imgui` (private, for `NESPalette::get_imgui_color`)
- `VibeNES_GUI` — main executable (links `vibes_core`, `imgui::imgui`, `opengl32`). Post-build step copies `SDL3.dll` next to the exe
- `VibeNES_Tests` — test executable (links `vibes_core` + `Catch2::Catch2WithMain` from vcpkg, `catch_discover_tests()` for per-test CTest)

ImGui is resolved from vcpkg (`find_package(imgui CONFIG REQUIRED)` → `imgui::imgui`) with the `sdl3-binding` feature — it is no longer a vendored static lib.

### MSVC-Specific Fixes Applied
- `src/main.cpp` has `#define SDL_MAIN_HANDLED` at line 1 (prevents SDL3 main hijack)
- `include/cartridge/rom_loader.hpp` has explicit `#include <array>` (MSVC doesn't transitively include it)
- `include/gui/panels/ppu_viewer_panel.hpp`, `src/gui/gui_application.cpp`, `src/gui/panels/ppu_viewer_panel.cpp` all have `#define NOMINMAX` + `#define WIN32_LEAN_AND_MEAN` + `#include <windows.h>` before `#include <GL/gl.h>`
- `src/ppu/nes_palette.cpp` — removed `#ifdef NES_GUI_ENABLED` guard; always compiles `get_imgui_color()` since `vibes_core` has the imgui include path
- `CMakeLists.txt` uses link options for subsystem (CONSOLE debug, WINDOWS release) instead of `WIN32` on `add_executable`
- All targets use generator expressions for debug/release compile options (no D9025 override warnings)

### Build Warnings
✅ **Zero warnings** — All C4244 narrowing warnings fixed with explicit `static_cast<int>()` in cpu_6502.cpp (3 sites where `CpuCycle::count()` returns `int64_t`).

### VS Code Integration
- `.vscode/tasks.json` — Shell tasks via `cmd.exe` with explicit MSVC paths (env vars `%VCVARS%`, `%CMAKE%`). No CMake Tools extension dependency for build/test.
- `.vscode/settings.json` — CMake Tools for IntelliSense only (`configurationProvider: "ms-vscode.cmake-tools"`). Includes "MSVC Developer CMD" terminal profile.
- `.vscode/launch.json` — Two `cppvsdbg` configurations: "Debug VibeNES" and "Debug Tests", both use `preLaunchTask: "Build Debug"`.
- No `c_cpp_properties.json` — CMake Tools provides IntelliSense via `compile_commands.json`.

## Current State (as of May 2026)
- **Phases 1-4 complete; Phase 5 substantially complete**. Debug and release builds green, **zero warnings**. MSYS2 fully removed from machine.
- **All 242 tests pass** (0 failures). 53 tests added in Phase 5: APU (all 5 channels), mappers 0-4, ROM loader, save states.
- **Test directories**: `tests/apu/`, `tests/cartridge/`, `tests/core/`, `tests/cpu/`, `tests/memory/`, `tests/ppu/` (plus `tests/input/`, `tests/fixtures/`, `tests/test_roms/`)
- **Code-review follow-ups (2026-05-29)**: save-state read helpers bounds-checked; PPU sprite-attribute unpack uses `std::bit_cast`; APU IRQ now level-triggered (bug #10); README/instructions refreshed for SDL3 and correct dependency versions.
- **Games tested**: Super Mario Bros., Crystalis, Guardian Legend.
- **Dependencies**: SDL3 + Catch2 + ImGui (sdl3-binding) all via vcpkg, x64-windows.
- **Remaining**: Add input tests. Add test ROM infrastructure. Address remaining minor PPU edge-case rendering quirks as accuracy work continues.

### MMC3 Fixes (Mar 1, 2026)
Four bugs fixed to make MMC3 games (Crystalis) playable:
1. **Bus IRQ line never deasserted** — `tick_single_cpu_cycle()` now computes combined IRQ from mapper + APU frame + APU DMC; calls `clear_irq_line()` when no sources active
2. **Sprite pattern fetches batched at cycle 257** — New `perform_sprite_fetch_cycle()` spreads reads across cycles 257-320 (8 cycles/sprite), producing correct A12 transitions
3. **BG fetches gated on BG-only enable** — Changed to unconditional within rendering-enabled blocks (sprites-only games get correct A12)
4. **Non-standard `irq_initialized_` guard in Mapper004** — Removed; real hardware has no such initialization requirement
