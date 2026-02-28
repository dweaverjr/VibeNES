# VibeNES - Cycle-Accurate NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in modern C++23. Features per-cycle CPU/PPU/APU interleaving, 5 mapper implementations, a full debug GUI, and 242 passing tests.

## Features

- **Cycle-accurate emulation** — Per-cycle CPU/PPU/APU interleaving via fat `consume_cycle()`. Each CPU cycle advances the PPU by 3 dots and the APU by 1 cycle.
- **Complete 6502 CPU** — All 256 opcodes (legal + illegal), hardware-accurate startup, penultimate-cycle interrupt polling (CLI delay, SEI window), OAM DMA halt (513 cycles), DMC DMA cycle stealing (~4 CPU stall cycles).
- **PPU rendering pipeline** — Dot-based rendering (341 dots × 262 scanlines), background + sprite evaluation, sprite 0 hit, scrolling, palette mirroring.
- **APU with all 5 channels** — Pulse ×2, Triangle, Noise, DMC. Frame counter (4-step/5-step modes), non-linear mixing, length counters, envelopes, sweep units.
- **5 cartridge mappers** — NROM (0), MMC1 (1), UxROM (2), CNROM (3), MMC3 (4) with bus conflict emulation and A12 IRQ filtering.
- **Save states** — Serialize/deserialize with CRC32 verification across all components.
- **Debug GUI** — SDL3 + ImGui with 7 panels: CPU state, disassembler, memory viewer, ROM info, PPU viewer, timing, audio.
- **Modern C++23** — `std::expected`, concepts, ranges, `constexpr`, strong types, RAII, no raw pointers.

## Games Tested

| Game | Mapper | Status |
|------|--------|--------|
| Super Mario Bros. | NROM (0) | ✅ Background + all sprites render correctly |
| Crystalis | MMC1 (1) | ✅ Boots and runs |

## Building

### Prerequisites

- **Visual Studio Build Tools 2022** (MSVC v143, C++23 support)
- **CMake 3.25+** and **Ninja** (bundled with Build Tools)
- **vcpkg** (bootstrapped locally in project — `./vcpkg/vcpkg.exe`)

Dependencies (installed automatically via vcpkg):
- SDL3 3.4.2
- Catch2 3.13.0
- ImGui 1.91.9

### Build Commands

Use VS Code tasks (Ctrl+Shift+B for default build) or the command line:

```cmd
:: Configure + build debug
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
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
| **Build Debug** (default) | Configure + build debug with MSVC + Ninja |
| **Build Release** | Configure + build release |
| **Run Tests** | Build debug + run all CTest suites |
| **Run VibeNES** | Build debug + launch GUI |
| **Clean** | Delete build directories |

### CMake Targets

| Target | Description |
|--------|-------------|
| `vibes_core` | Static library — CPU, PPU, APU, Bus, Cartridge, Input, SaveState |
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
| CPU (6502) | ~2200 | 247 explicit opcodes + 9 catch-all NOPs, all 256 handled |
| PPU (2C02) | ~1800 | Full rendering pipeline, sprite evaluation, scrolling |
| APU (2A03) | ~1050 | All 5 channels inline in header, frame counter, non-linear mixing |
| Bus | ~500 | Full NES memory map, mirroring, open bus, DMA |
| Mappers 0–4 | ~1200 | NROM, MMC1, UxROM, CNROM, MMC3 |
| Save States | ~320 | Serialize/deserialize with CRC32 |
| GUI | ~2000 | SDL3 + ImGui, 7 debug panels, retro theme |
| Disassembler | ~400 | All 256 opcodes with addressing modes |

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
│   └── system/         save_state.hpp
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
└── roms/               Test ROM files
```

## Testing

**242 tests, all passing.** Catch2 v3 via vcpkg with `catch_discover_tests()` for per-TEST_CASE CTest entries.

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

- **APU uses edge-triggered IRQ** — NES APU IRQ is level-triggered
- **MMC3 sprite fetch timing** — Batched at cycle 257 instead of per-sprite
- **Minor PPU rendering issues** — Some edge-case rendering bugs remain

## License

See [LICENSE](LICENSE) for details.
