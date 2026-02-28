# VibeNES - Cycle-Accurate NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in C++23, designed for perfect game compatibility and hardware accuracy.

## Project Goals
- **Cycle-accurate emulation**: Emulate NES hardware behavior cycle-by-cycle for perfect game compatibility
- **Modern C++**: Use C++23 features extensively (modules, ranges, concepts, etc.)
- **Clean architecture**: Modular design with separate CPU, PPU, APU, and mapper components
- **Performance**: Maintain real-time performance while achieving hardware accuracy

## Current Status

### ✅ CPU (6502) - **Complete Implementation**
- [x] Complete instruction execution framework with all 256 opcodes
- [x] All addressing modes (immediate, absolute, zero page, indexed, indirect)
- [x] Page boundary crossing detection and cycle penalties
- [x] Cycle-accurate timing for all instructions and addressing modes
- [x] Hardware-accurate startup behavior and reset sequence
- [x] **Complete 6502 instruction set including illegal opcodes**
  - [x] Load/Store Operations ✅
  - [x] Arithmetic Operations ✅
  - [x] Logical Operations ✅
  - [x] Shift/Rotate Operations ✅
  - [x] Compare Operations ✅
  - [x] Transfer Operations ✅
  - [x] Increment/Decrement Operations ✅
  - [x] Branch Operations ✅
  - [x] Jump/Subroutine Operations ✅
  - [x] Stack Operations ✅
  - [x] Status Flag Operations ✅
  - [x] System Operations ✅
  - [x] No Operation ✅

### ✅ Memory System - **Complete**
- [x] System bus with proper address decoding
- [x] RAM with mirroring (0x0000-0x1FFF)
- [x] PPU register mapping (0x2000-0x3FFF)
- [x] APU/Controller register mapping (0x4000-0x401F)
- [x] Dual-purpose register handling (APU Frame Counter / Controller 2)
- [x] Open bus behavior for unmapped regions
- [x] Cartridge memory mapping (0x4020-0xFFFF)

### ✅ PPU (2C02) - **Complete Phase 1-4 Implementation**
- [x] Complete PPU register interface with proper timing
- [x] Cycle-accurate dot-based rendering pipeline (341 dots × 262 scanlines)
- [x] Background rendering with nametables and pattern tables
- [x] Sprite evaluation and rendering with sprite 0 hit detection
- [x] Advanced NES scrolling system with proper VRAM address handling
- [x] Palette system with authentic NES colors
- [x] Frame buffer generation for display output

### ✅ Cartridge System - **Complete with iNES Support**
- [x] iNES ROM file format parsing with header validation
- [x] Mapper 0 (NROM) complete implementation
- [x] Cartridge factory pattern for extensible mapper support
- [x] CHR ROM/RAM access for PPU pattern tables
- [x] PRG ROM bank management
- [x] GUI file browser integration for ROM loading

### ✅ Debug System - **Complete GUI Framework**
- [x] SDL2 + ImGui integration with retro aesthetics
- [x] Real-time CPU state monitoring (registers, flags, stack)
- [x] Interactive memory viewer with search and navigation
- [x] Complete 6502 disassembler (all 256 opcodes with addressing modes)
- [x] Step-by-step CPU execution controls
- [x] Dockable panel system for flexible debugging layout
- [x] ROM loading interface with drag-and-drop support

### ⚠️ APU (2A03) - **Stub Implementation**
- [x] Register interface for CPU compatibility
- [x] Frame counter basic functionality
- [ ] Audio channel implementations (pulse, triangle, noise, DMC)
- [ ] Audio mixing and output
- [ ] Sample-accurate synthesis

### 🎯 Next Priority: Audio Implementation**
- [ ] Complete APU channel implementations
- [ ] Audio output integration
- [ ] Frame buffer display in GUI
- [ ] Additional mapper support (MMC1, MMC3)
- [ ] Save state functionality

## Key Features Implemented

### Complete 6502 CPU Emulation
- **All 256 opcodes**: Legal and illegal instructions with proper cycle timing
- **Hardware accuracy**: Authentic startup behavior and reset sequences
- **Comprehensive disassembler**: Real-time instruction analysis with all addressing modes
- **Interactive debugging**: Step-by-step execution with register monitoring

### Advanced PPU Graphics System
- **Cycle-accurate rendering**: 341-dot scanline timing with proper frame structure
- **Complete background system**: Nametable rendering with pattern table lookups
- **Sprite system**: 8-sprite-per-scanline evaluation with sprite 0 hit detection
- **NES scrolling**: Authentic 15-bit VRAM address register system
- **Palette support**: Hardware-accurate color generation

### Robust Cartridge System
- **iNES ROM format**: Complete header parsing and validation
- **Mapper 0 (NROM)**: Full implementation with CHR/PRG ROM support
- **Extensible design**: Factory pattern ready for additional mappers
- **GUI integration**: Drag-and-drop ROM loading interface

### Professional Debug Interface
- **Modern GUI**: SDL2 + ImGui with authentic retro styling
- **Real-time monitoring**: Live CPU state, memory contents, and execution flow
- **Interactive tools**: Memory search, navigation, and hex editing capabilities
- **Modular panels**: Dockable interface for customizable debugging layout

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
- Visual Studio Build Tools 2022 (MSVC v143, C++23)
- CMake 3.25+ and Ninja (bundled with Build Tools)
- VS Code with CMake Tools and C/C++ extensions
- vcpkg (bootstrapped locally in project)

### Build Commands
```powershell
# Configure (first time or after CMakeLists.txt changes)
cmake --preset debug

# Build
cmake --build --preset debug

# Run tests
ctest --preset debug

# Or use VS Code: F7 to build, F5 to debug
```

### Quick Start
1. Open project in VS Code
2. CMake Tools auto-configures on open
3. Press F7 to build, F5 to debug
4. Load a ROM file using the GUI interface
5. Use CPU debugging controls to step through execution

## Screenshots

### Main Debug Interface
*Complete debugging environment with CPU state, memory viewer, and disassembler*

### PPU Visualization
*Real-time graphics rendering with pattern table and nametable display*

### ROM Loading
*Integrated ROM browser with iNES header validation*

## Technical Achievements

### CPU Implementation Highlights
- **Complete instruction set**: All 256 opcodes including undocumented instructions
- **Cycle accuracy**: Proper timing for all addressing modes and page boundary penalties
- **Hardware fidelity**: Authentic power-on state and reset behavior
- **Debug integration**: Real-time register monitoring and step execution

### PPU Implementation Highlights
- **Phase-based development**: Systematic implementation following NES PPU specification
- **Authentic scrolling**: Complex 15-bit VRAM address register system
- **Sprite accuracy**: Proper 8-sprite evaluation with sprite 0 hit timing
- **Memory integration**: Complete PPU memory map with proper mirroring

### Software Engineering Excellence
- **Modern C++23**: Extensive use of concepts, ranges, and strong typing
- **Component architecture**: Modular design with clean interfaces
- **Comprehensive testing**: Extensive test coverage for CPU instruction accuracy
- **Professional tooling**: Complete debugging suite with GUI integration

## Architecture Overview

This structure provides clear separation of concerns while maintaining the tight coupling needed for cycle-accurate emulation. Each component can be developed and tested independently while ensuring proper system-wide timing.

The design emphasizes:
- **Accuracy over speed** (initially) - Hardware-faithful emulation
- **Modularity** for easy testing and debugging
- **Modern C++** for maintainable and efficient code
- **Extensibility** for adding new mappers and features
- **Developer experience** with comprehensive debugging tools
