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

## Timing Model

The emulator uses a master clock that drives all components. Each component implements the `Clockable` concept and receives clock ticks to advance its state.

## Communication

Components communicate through:
- System bus for memory-mapped I/O
- Direct connections for special cases (DMA)
- Event system for debugging and logging
