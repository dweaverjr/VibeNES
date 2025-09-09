# VibeNES - Cycle-Accurate NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in C++23, designed for perfect game compatibility and hardware accuracy.

## Project Goals
- **Cycle-accurate emulation**: Emulate NES hardware behavior cycle-by-cycle for perfect game compatibility
- **Modern C++**: Use C++23 features extensively (modules, ranges, concepts, etc.)
- **Clean architecture**: Modular design with separate CPU, PPU, APU, and mapper components
- **Performance**: Maintain real-time performance while achieving hardware accuracy

## Current Status

### ✅ CPU (6502) - **10/10 Timing Accuracy**
- [x] Basic instruction execution framework
- [x] Core addressing modes (immediate, absolute, zero page, indexed, indirect)
- [x] Page boundary crossing detection and penalties
- [x] Cycle-accurate timing for all addressing modes
- [x] Comprehensive test coverage with edge case validation
- [x] **146/151 official opcodes implemented (96.7% complete)**
  - [x] Load/Store Operations (32 opcodes) ✅
  - [x] Arithmetic Operations (16 opcodes) ✅
  - [x] Logical Operations (24 opcodes) ✅
  - [x] Shift/Rotate Operations (20 opcodes) ✅
  - [x] Compare Operations (9 opcodes) ✅
  - [x] Transfer Operations (4/6 opcodes) - Missing TXS, TSX
  - [x] Increment/Decrement Operations (12 opcodes) ✅
  - [x] Branch Operations (8 opcodes) ✅
  - [x] Jump/Subroutine Operations (5 opcodes) ✅
  - [x] Stack Operations (4 opcodes) ✅
  - [x] Status Flag Operations (7 opcodes) ✅
  - [x] System Operations (1/4 opcodes) - Missing BRK, BIT (2 variants)
  - [x] No Operation (1 opcode) ✅
- [ ] Complete instruction set (5 opcodes remaining)
- [ ] Interrupt handling (IRQ/NMI)

### 🚧 Memory System
- [x] System bus with proper address decoding
- [x] RAM with mirroring (0x0000-0x1FFF)
- [x] Open bus behavior for unmapped regions
- [ ] PPU memory integration
- [ ] Cartridge memory mapping

### 📋 PPU (2C02) - Planned
- [ ] Basic register interface
- [ ] Dot-based rendering pipeline
- [ ] Sprite evaluation and rendering
- [ ] Background tile rendering

### 📋 APU (2A03) - Planned
- [ ] Audio channel implementations
- [ ] Frame counter and sequencing
- [ ] Audio mixing and output

### 📋 Cartridge System - Planned
- [ ] ROM loading (iNES format)
- [ ] Basic mappers (000, 001, 002, 003, 004)
- [ ] Bank switching mechanics

## Architecture Components
- **CPU (6502)**: Cycle-accurate 6502 processor emulation
- **PPU (Picture Processing Unit)**: Pixel-perfect graphics rendering
- **APU (Audio Processing Unit)**: Accurate sound synthesis
- **Mappers**: Various cartridge mapper implementations
- **Bus**: Memory mapping and component interconnection

## Detailed Project Structure

```
VibeNES/
├── include/
│   ├── core/
│   │   ├── types.hpp           # Strong types, concepts, common definitions
│   │   ├── clock.hpp           # Master clock and synchronization
│   │   ├── bus.hpp             # System bus interface
│   │   └── component.hpp       # Base component interface
│   │
│   ├── cpu/
│   │   ├── cpu_6502.hpp        # Main CPU class
│   │   ├── instructions.hpp    # Instruction set definitions
│   │   ├── addressing_modes.hpp # Addressing mode handlers
│   │   ├── registers.hpp       # CPU register definitions
│   │   └── interrupts.hpp      # IRQ/NMI handling
│   │
│   ├── ppu/
│   │   ├── ppu_2c02.hpp        # Main PPU class
│   │   ├── registers.hpp       # PPU register definitions
│   │   ├── rendering.hpp       # Rendering pipeline
│   │   ├── sprites.hpp         # Sprite evaluation and rendering
│   │   ├── background.hpp      # Background rendering
│   │   ├── palette.hpp         # Color palette management
│   │   └── frame_buffer.hpp    # Frame buffer management
│   │
│   ├── apu/
│   │   ├── apu_2a03.hpp        # Main APU class
│   │   ├── channels/
│   │   │   ├── pulse.hpp       # Pulse wave channels
│   │   │   ├── triangle.hpp    # Triangle wave channel
│   │   │   ├── noise.hpp       # Noise channel
│   │   │   └── dmc.hpp         # Delta modulation channel
│   │   ├── mixer.hpp           # Audio mixing
│   │   └── frame_counter.hpp   # Frame sequencer
│   │
│   ├── memory/
│   │   ├── memory_map.hpp      # Memory mapping definitions
│   │   ├── ram.hpp             # Work RAM (2KB)
│   │   ├── ppu_memory.hpp      # PPU memory (VRAM, OAM)
│   │   └── dma.hpp             # DMA controller
│   │
│   ├── cartridge/
│   │   ├── cartridge.hpp       # Cartridge interface
│   │   ├── rom_loader.hpp      # ROM file parsing (iNES/NES2.0)
│   │   ├── mappers/
│   │   │   ├── mapper.hpp      # Base mapper interface
│   │   │   ├── mapper_000.hpp  # NROM
│   │   │   ├── mapper_001.hpp  # MMC1
│   │   │   ├── mapper_002.hpp  # UxROM
│   │   │   ├── mapper_003.hpp  # CNROM
│   │   │   ├── mapper_004.hpp  # MMC3
│   │   │   └── ...             # Other mappers
│   │   └── mapper_factory.hpp  # Mapper creation
│   │
│   ├── input/
│   │   ├── controller.hpp      # Controller interface
│   │   ├── standard_controller.hpp
│   │   └── zapper.hpp          # Light gun support
│   │
│   ├── system/
│   │   ├── nes_system.hpp      # Main system class
│   │   ├── reset_manager.hpp   # System reset handling
│   │   └── save_state.hpp     # Save state support
│   │
│   └── debug/
│       ├── debugger.hpp        # Debug interface
│       ├── disassembler.hpp    # 6502 disassembler
│       ├── memory_viewer.hpp   # Memory inspection
│       ├── ppu_viewer.hpp      # PPU state viewer
│       └── trace_logger.hpp    # Execution tracing
│
├── src/
│   ├── core/
│   ├── cpu/
│   ├── ppu/
│   ├── apu/
│   ├── memory/
│   ├── cartridge/
│   ├── input/
│   ├── system/
│   ├── debug/
│   └── main.cpp
│
├── tests/
│   ├── cpu/
│   │   ├── instruction_tests.cpp
│   │   ├── timing_tests.cpp
│   │   └── nestest_validation.cpp
│   ├── ppu/
│   │   ├── rendering_tests.cpp
│   │   └── timing_tests.cpp
│   ├── test_roms/
│   │   ├── nestest.nes
│   │   ├── ppu_tests/
│   │   └── apu_tests/
│   └── fixtures/
│
├── tools/
│   ├── rom_analyzer.cpp        # ROM analysis tool
│   └── palette_generator.cpp   # PAL/NTSC palette generation
│
└── docs/
    ├── architecture.md
    ├── timing_notes.md
    └── mapper_notes.md
```

## Key Component Details

### Core Types (`include/core/types.hpp`)
```cpp
namespace nes {
    // Strong type for cycles
    using Cycle = std::chrono::duration<std::int64_t, std::ratio<1, 1'789'773>>;

    // Memory addresses
    using Address = std::uint16_t;
    using Byte = std::uint8_t;

    // Concepts
    template<typename T>
    concept Clockable = requires(T t, Cycle cycles) {
        { t.tick(cycles) } -> std::same_as<void>;
    };
}
```

### Component Interface (`include/core/component.hpp`)
```cpp
class Component {
public:
    virtual void tick(Cycle cycles) = 0;
    virtual void reset() = 0;
    virtual void power_on() = 0;
};
```

### System Bus (`include/core/bus.hpp`)
```cpp
class SystemBus {
    // CPU memory map ($0000-$FFFF)
    // PPU registers ($2000-$2007)
    // APU registers ($4000-$4017)
    // Cartridge space ($4020-$FFFF)
};
```

### CPU Structure
- **Separate instruction handling** for each addressing mode
- **Cycle-accurate timing** with proper memory access patterns
- **Page boundary crossing** detection
- **Interrupt timing** with proper hijacking

### PPU Structure
- **Dot-based rendering** (341 dots per scanline)
- **Proper sprite evaluation** with overflow
- **Mid-frame register changes** support
- **Accurate VRAM access timing**

### APU Structure
- **Sample-accurate synthesis**
- **Proper frame counter** modes
- **DMC DMA** conflicts with CPU

### Mapper Architecture
- **Interface-based design** for extensibility
- **Cycle-accurate IRQ** generation (MMC3)
- **Proper bus conflicts** emulation

## Key Design Patterns

1. **Component-based**: Each hardware component inherits from `Component`
2. **Message passing**: Components communicate through the bus
3. **Strong typing**: Use C++23 concepts and strong types
4. **RAII**: Resource management for memory regions
5. **Factory pattern**: For mapper creation
6. **Observer pattern**: For debugging hooks

## Building

### Prerequisites
- MSYS2 with GCC 13+ (C++23 support)
- VS Code with C/C++ extension

### Build Commands
```bash
# Debug build
Ctrl+Shift+P -> "Tasks: Run Task" -> "Debug Build"

# Release build
Ctrl+Shift+P -> "Tasks: Run Task" -> "Release Build"

# Or use F5 to build and debug
```

## Architecture Overview

This structure provides clear separation of concerns while maintaining the tight coupling needed for cycle-accurate emulation. Each component can be developed and tested independently while ensuring proper system-wide timing.

The design emphasizes:
- **Accuracy over speed** (initially)
- **Modularity** for easy testing and debugging
- **Modern C++** for maintainable code
- **Extensibility** for adding new mappers and features
