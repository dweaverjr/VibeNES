# VibeNES - Cycle-Accurate NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in modern C++23. Features per-cycle CPU/PPU/APU interleaving, 5 mapper implementations, a full debug GUI, and 242 passing tests.

## Features

- **Cycle-accurate emulation** — Per-cycle CPU/PPU/APU interleaving via fat `consume_cycle()`. Each CPU cycle advances the PPU by 3 dots and the APU by 1 cycle.
- **Complete 6502 CPU** — All 256 opcodes (legal + illegal: 247 explicit + 9 catch-all NOPs), hardware-accurate startup, penultimate-cycle interrupt polling (CLI delay, SEI window), OAM DMA halt (513 cycles), DMC DMA cycle stealing (~4 CPU stall cycles).
- **PPU rendering pipeline** — Dot-based rendering (341 dots × 262 scanlines), background + sprite evaluation, sprite 0 hit, scrolling, palette mirroring.
- **APU with all 5 channels** — Pulse ×2, Triangle, Noise, DMC. Frame counter (4-step/5-step modes), non-linear mixing, length counters, envelopes, sweep units, level-triggered IRQ. SDL3 audio output with sample-rate conversion.
- **5 cartridge mappers** — NROM (0), MMC1 (1), UxROM (2), CNROM (3), MMC3 (4) with bus-conflict emulation and MMC3 A12 IRQ filtering.
- **Save states** — Nine slots plus quick save/load. Serialize/deserialize with CRC32 ROM verification across all components.
- **Battery-backed saves (`.sav`)** — Native cartridge-battery emulation for MMC1/MMC3 carts flagged in the iNES header: SRAM is restored on load, preserved across reset, and written back on power-off (with a periodic crash-safety flush). Stored separately from save states.
- **Game controller input** — SDL3 gamepad support for Players 1 & 2 with runtime hot-plug detection.
- **Debug GUI** — SDL3 + ImGui interface: CPU state, disassembler, RAM/memory viewer, ROM loader, PPU viewer (NES display, pattern tables, palettes, registers & timing), and audio controls. Retro theme, fullscreen mode, and an optional CRT filter (scanlines, curvature, vignette, NTSC aspect correction).
- **Modern C++23** — `std::expected`, concepts, `std::span`, `constexpr`, strong types, RAII, no raw pointers.

## Controls

Gameplay requires a game controller (SDL3 gamepad). Keyboard keys drive emulator functions only.

### NES controller (gamepad)

| NES button | Gamepad |
|------------|---------|
| D-pad | D-pad |
| A | A (south) |
| B | X (west) |
| Start | Start |
| Select | Back / Select |

Two controllers are supported (Player 1 and Player 2) with hot-plugging.

### Emulator hotkeys

| Key | Action |
|-----|--------|
| F1–F9 | Save state to slot 1–9 |
| Shift+F1–F9 | Load state from slot 1–9 |
| Ctrl+F5 | Quick save |
| Ctrl+F8 | Quick load |
| F11 / Alt+Enter | Toggle fullscreen |
| Esc | Exit fullscreen |

## Building

### Prerequisites

- **Visual Studio Build Tools 2022 or newer** (MSVC with C++23 support). The build script auto-detects VS 2019/2022/2026, any edition.
- **CMake 3.25+** and **Ninja** (bundled with the Build Tools)
- **vcpkg** (bootstrapped locally in the project — `./vcpkg/vcpkg.exe`)

Dependencies are installed automatically via vcpkg (versions come from the project-local vcpkg checkout in `./vcpkg`):
- SDL3 (3.4.x)
- Catch2 3.x
- ImGui 1.92.x (with `sdl3-binding` + `opengl3-binding` features)

### Build Commands

Use the VS Code tasks (Ctrl+Shift+B for the default build) or the command line. `.vscode\vsdev.bat` locates the newest installed MSVC and puts the bundled CMake + Ninja on `PATH`:

```cmd
:: Configure + build debug
call .vscode\vsdev.bat
cmake --preset debug
cmake --build --preset debug

:: Run tests
cmake -E chdir build/debug ctest --output-on-failure

:: Build + run the emulator
cmake --build --preset debug
.\build\debug\VibeNES_GUI.exe
```

### VS Code Tasks

| Task | Description |
|------|-------------|
| **Build Debug** (default) | Configure + build debug (MSVC + Ninja) |
| **Build Release** | Configure + build release |
| **Run Tests** | Build debug + run all CTest suites |
| **Run Tests: CPU** / **Run Tests: PPU** | Build debug + run a single suite |
| **Run VibeNES (Debug)** / **Run VibeNES (Release)** | Build + launch the GUI |
| **Clean** | Delete build directories |
| **Package Release** | Build release + CPack ZIP (and NSIS installer if `makensis` is present) |

### CMake Targets

| Target | Description |
|--------|-------------|
| `vibes_core` | Static library — CPU, PPU, APU, Bus, Cartridge, Input, save states, battery saves |
| `VibeNES_GUI` | Main executable — links vibes_core, imgui, opengl32 |
| `VibeNES_Tests` | Test executable — links vibes_core + Catch2, `catch_discover_tests()` |

## Architecture

### Synchronization Model

Each `consume_cycle()` call inside CPU instructions calls `bus_->tick_single_cpu_cycle()`, which:
1. Advances PPU by 3 dots
2. Advances APU by 1 cycle
3. Checks mapper IRQs
4. Samples NMI/IRQ lines for penultimate-cycle interrupt polling
5. Checks for pending DMC DMA requests

The main loop calls `execute_instruction()` which returns the consumed cycle count.

### Component Overview

| Component | Lines | Description |
|-----------|-------|-------------|
| CPU (6502) | ~5,900 | 247 explicit opcodes + 9 catch-all NOPs (all 256 handled), interrupts |
| PPU (2C02) | ~3,500 | Full rendering pipeline, sprite evaluation, scrolling, palette |
| APU (2A03) | ~1,500 | All 5 channels (inline in header), frame counter, non-linear mixing |
| Bus / memory | ~670 | Full NES memory map, mirroring, open bus, DMA |
| Mappers 0–4 | ~1,650 | NROM, MMC1, UxROM, CNROM, MMC3 |
| Cartridge / ROM loader | ~570 | iNES parsing, mapper factory |
| Save states | ~530 | Serialize/deserialize with CRC32 verification |
| Battery saves | ~200 | Native `.sav` persistence for MMC1/MMC3 |
| GUI (incl. disassembler) | ~4,600 | SDL3 + ImGui debug panels, retro theme, CRT filter |

Roughly 21,000 lines of C++23 across `src/` and `include/`.

### Memory Map

```
CPU:  $0000-$07FF  2KB RAM (mirrored ×4 to $1FFF)
      $2000-$2007  PPU registers (mirrored to $3FFF)
      $4000-$4017  APU/IO registers
      $4020-$FFFF  Cartridge space (PRG ROM/RAM)

PPU:  $0000-$1FFF  Pattern tables (CHR ROM/RAM)
      $2000-$2FFF  Nametables (2KB VRAM)
      $3F00-$3F1F  Palette RAM (25 usable colors)
```

### NES Specifications

- **CPU**: 6502 at ~1.789773 MHz (NTSC), 1 CPU cycle = 3 PPU dots
- **PPU**: 341 dots/scanline, 262 scanlines/frame, 256×240 visible
- **Memory**: 2KB work RAM, 2KB VRAM, 256B OAM
- **Palette**: 64 colors, 25 usable simultaneously

## Project Structure

```
VibeNES/
├── include/
│   ├── apu/            apu.hpp
│   ├── audio/          audio_backend.hpp, sample_rate_converter.hpp
│   ├── cartridge/      cartridge.hpp, rom_loader.hpp, mapper_factory.hpp
│   │   └── mappers/    mapper_000–004.hpp
│   ├── core/           bus.hpp, component.hpp, types.hpp
│   ├── cpu/            cpu_6502.hpp, interrupts.hpp
│   ├── gui/            gui_application.hpp + panels/ + style/
│   ├── input/          controller.hpp, gamepad_manager.hpp
│   ├── memory/         ram.hpp
│   ├── ppu/            ppu.hpp, ppu_registers.hpp, ppu_memory.hpp, nes_palette.hpp
│   └── system/         save_state.hpp, battery_save.hpp
├── src/                Implementations matching include/ layout
├── tests/
│   ├── apu/            APU channel, register, mixing, serialization tests
│   ├── cartridge/      Mapper 0–4, ROM loader, save state tests
│   ├── core/           Bus, component tests
│   ├── cpu/            Instruction, timing, interrupt tests
│   ├── memory/         RAM mirroring tests
│   └── ppu/            Rendering, scrolling, sprite, register tests
├── vcpkg/              Project-local vcpkg installation
├── docs/               Architecture, timing, mapper notes
├── roms/               Default ROM directory (place .nes files here)
└── saves/              Save-state (.vns) and battery (battery/*.sav) files
```

## Testing

**242 tests, all passing.** Catch2 v3 via vcpkg with `catch_discover_tests()` for per-TEST_CASE CTest entries. The per-area split below is approximate; 242 is the exact total.

| Area | Tests | Coverage |
|------|-------|----------|
| CPU | ~80 | All instruction groups, addressing modes, timing, interrupts |
| PPU | ~90 | Rendering pipeline, registers, scrolling, sprites, palette |
| APU | ~40 | All 5 channels, frame counter, mixing, serialization, DMA |
| Mappers | ~20 | NROM, MMC1, UxROM, CNROM, MMC3 — banking, IRQ, serialization |
| Cartridge | ~10 | iNES parsing, ROM loading, save state header validation |
| Core | ~5 | Bus, memory, type system |

Run tests:
```cmd
cmake --build --preset debug
cmake -E chdir build/debug ctest --output-on-failure
```

## Known Issues

- **Gameplay input is gamepad-only** — NES controllers map to SDL3 game controllers; there is no keyboard mapping for in-game input (the keyboard drives emulator hotkeys only).
- **Minor PPU rendering issues** — Some edge-case rendering bugs remain (e.g. obscure mid-scanline register-write corner cases).
- **Bus-conflict emulation is simplified** — Mapper 2/3 bus conflicts read from the currently selected bank rather than the bank fixed at the written address.

## License

See [LICENSE](LICENSE) for details.
