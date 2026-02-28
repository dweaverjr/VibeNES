# VibeNES Architecture Documentation

This document describes the internal architecture of the VibeNES emulator.

## Overview

VibeNES is designed as a cycle-accurate NES emulator with modular components that communicate through a system bus.

## Component Architecture

### Core Components
- Clock management and synchronization
- System bus for component communication
- Base component interface

### CPU (6502)
- Cycle-accurate instruction execution
- Proper timing for memory accesses
- Interrupt handling (IRQ/NMI)

### PPU (2C02)
- Dot-based rendering pipeline
- Sprite evaluation and rendering
- Background tile rendering
- Palette management

### APU (2A03)
- Sample-accurate audio synthesis
- Frame counter management
- Audio mixing

### Memory Management
- CPU address space mapping
- PPU memory management
- DMA controller

### Cartridge System
- ROM loading and parsing
- Mapper implementations
- Bank switching

## Design Principles

1. **Accuracy First**: Prioritize hardware accuracy over performance
2. **Modularity**: Clear separation between components
3. **Testability**: Each component can be tested independently
4. **Extensibility**: Easy to add new mappers and features

## Development Practices

### Testing Strategy
- Use exact cycle counts in tests to avoid false positives from extra instructions
- Test memory accesses within appropriate address ranges (RAM: 0x0000-0x1FFF)
- Comprehensive edge case coverage for boundary conditions
- Separate test sections for different scenarios (normal operation vs. edge cases)

### Test Compilation
Tests are built via CMake as the `VibeNES_Tests` target:

```powershell
cmake --preset debug
cmake --build --preset debug --target VibeNES_Tests
ctest --preset debug
```

New test files in `tests/` are auto-discovered by `GLOB_RECURSE` in CMakeLists.txt. Source files are compiled into `vibes_core` and linked automatically.

### CPU Implementation Guidelines
- Manual PC management for multi-byte instructions (avoid helper functions that auto-increment)
- Explicit cycle consumption for timing accuracy
- Helper functions for common operations (e.g., page boundary detection)
- Clear separation between instruction decode and execution phases

### Memory System Design
- Open bus behavior for unmapped regions
- Proper address space partitioning (RAM, PPU, APU, etc.)
- Component-specific handling of out-of-range accesses

## Timing Model

The emulator uses a master clock that drives all components. Each component implements the `Clockable` concept and receives clock ticks to advance its state.

## Communication

Components communicate through:
- System bus for memory-mapped I/O
- Direct connections for special cases (DMA)
- Event system for debugging and logging
