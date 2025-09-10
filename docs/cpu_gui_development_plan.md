# CPU State GUI Development Plan

## Project Overview
Development of a CPU state visualization and debugging GUI for the VibeNES emulator, focusing on real-time CPU state monitoring, instruction disassembly, and debugging capabilities.

## Development Phases

### Phase 1: Complete Memory Map ✅ **COMPLETED**
**Duration**: 1-2 weeks  
**Status**: ✅ COMPLETE

**Objectives**:
- ✅ Expand SystemBus to handle full $0000-$FFFF range
- ✅ Create stub classes for PPU, APU, Controllers, Cartridge
- ✅ Add proper address mirroring (RAM mirrors every $800, PPU every $8, etc.)

**Completed Implementation**:
- **SystemBus**: Complete NES memory map integration with full $0000-$FFFF coverage
- **PPUStub** (`include/ppu/ppu_stub.hpp`): PPU register simulation with 8-byte mirroring
- **APUStub** (`include/apu/apu_stub.hpp`): APU register handling for $4000-$401F
- **ControllerStub** (`include/input/controller_stub.hpp`): Controller input simulation for $4016-$4017
- **CartridgeStub** (`include/cartridge/cartridge_stub.hpp`): Cartridge memory management for $4020-$FFFF
- **Address Mirroring**: RAM ($0000-$1FFF), PPU registers ($2000-$3FFF), proper component routing

### Phase 2: Basic GUI Framework 🔄 **NEXT**
**Duration**: 2-3 weeks  
**Status**: 🔄 PLANNING

**Technology Decisions**:
- **GUI Library**: ImGui + SDL2
- **Platform**: Cross-platform (Windows-first development)
- **Visual Style**: Retro/authentic look
- **Priority Features**: CPU state display and instruction disassembler
- **Development Environment**: MSYS2/MinGW with additional dependencies

**Objectives**:
- Setup ImGui + SDL2 integration
- Create main window with retro styling
- Implement dockable panel system
- Basic CPU state display framework
- Execution control interface (play/pause/step)

**Detailed Tasks**:
1. **Dependencies Setup**
   - Add SDL2 to MSYS2 environment
   - Integrate ImGui into build system
   - Setup OpenGL context
   
2. **Main Window Structure**
   - SDL2 window creation and management
   - ImGui integration and initialization
   - Retro color scheme and fonts
   - Docking system for panels
   
3. **Basic Framework**
   - Abstract panel base class
   - Panel registration system
   - Main application loop
   - Event handling

4. **Initial CPU Interface**
   - Basic CPU state display panel
   - Connect to CPU_6502 class
   - Simple register visualization
   - Foundation for disassembler

### Phase 3: CPU State Display 📋 **PLANNED**
**Duration**: 1-2 weeks

**Objectives**:
- Complete CPU register visualization
- Status flags display with individual indicators
- Stack memory visualization
- Current instruction display with disassembly
- Step-by-step execution control
- Real-time data binding to CPU_6502

**Features**:
- **Register Display**: A, X, Y, SP, PC with hex/decimal formats
- **Status Flags**: Visual indicators for N, V, B, D, I, Z, C flags
- **Stack Viewer**: Current stack contents with SP highlighting
- **Disassembler**: Current instruction + next few instructions
- **Execution Control**: Single-step, run/pause, reset
- **Memory Integration**: Live updates from SystemBus

### Phase 4+: Extended Features 🎯 **FUTURE**
- Memory browser and editor
- PPU state visualization
- APU state monitoring
- Breakpoint system
- Watch expressions
- Performance profiling

## Technical Architecture

### Memory Map Coverage
```
$0000-$1FFF: RAM [with mirroring every $800]
$2000-$3FFF: PPU registers [8-byte mirroring] 
$4000-$401F: APU/IO registers
$4016-$4017: Controller ports
$4020-$FFFF: Cartridge space (SRAM + PRG ROM)
```

### Component Integration
- **SystemBus**: Central memory routing to all components
- **CPU_6502**: Register access and execution state
- **Component Architecture**: Shared lifecycle (tick/reset/power_on)
- **Address Decoding**: Proper helper methods for each memory region

### GUI Architecture (Phase 2+)
- **ImGui + SDL2**: Cross-platform immediate mode GUI
- **Docking System**: Flexible panel layout
- **Retro Styling**: Authentic look with period-appropriate colors/fonts
- **Real-time Updates**: Live CPU state monitoring
- **Panel System**: Modular, extensible interface components

## Development Notes

### PPU Register Mirroring
PPU registers ($2000-$3FFF) mirror every 8 bytes:
- Only 8 actual registers ($2000-$2007)
- $2008-$200F maps to same registers as $2000-$2007
- Mirroring function: `0x2000 + (address % 8)`
- Hardware simplification from incomplete address decoding

### CPU-PPU Data Communication
CPU writes to PPU through register interface:
- $2006 (PPUADDR): Set PPU memory address (written twice: high, low)
- $2007 (PPUDATA): Write data to PPU memory at current address
- $4014 (OAMDMA): DMA transfer from CPU RAM to sprite memory
- PPU has separate memory space, not directly accessible by CPU

### Platform Considerations
- **Cross-platform**: SDL2 provides portable windowing/input
- **Build System**: CMake for multi-platform builds
- **File Paths**: std::filesystem for portable path handling
- **Dependencies**: Package manager integration (vcpkg/Conan)

## Current Status
- ✅ Phase 1 Complete: Full memory map with component stubs
- 🔄 Phase 2 Next: GUI framework setup
- 📅 Target: CPU debugging interface by end of Phase 3

## Repository Structure
```
include/
├── core/bus.hpp          # SystemBus with complete memory map
├── ppu/ppu_stub.hpp      # PPU register simulation
├── apu/apu_stub.hpp      # APU register handling  
├── input/controller_stub.hpp  # Controller interface
├── cartridge/cartridge_stub.hpp  # Cartridge memory
└── gui/                  # GUI components (Phase 2+)

src/
├── core/bus.cpp          # Memory routing implementation
├── main.cpp             # Application entry point
└── gui/                 # GUI implementation (Phase 2+)
```
