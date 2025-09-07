# NES Timing Notes

This document contains detailed timing information for NES hardware components.

## CPU Timing (6502)

### Basic Operation
- CPU runs at 1.789773 MHz (NTSC)
- Each instruction takes 2-7 cycles
- Memory access adds cycles

### Page Boundary Crossing
- Occurs when low byte of address overflows (high byte changes)
- Detection: `(base_address & 0xFF00) != ((base_address + offset) & 0xFF00)`
- Adds 1 cycle to certain addressing modes
- Affects: absolute,X; absolute,Y; indirect,Y
- Implementation note: Use bitwise operations for efficient detection
- Testing: Ensure tests use RAM addresses (0x0000-0x1FFF) to avoid bus conflicts

### Interrupt Timing
- IRQ: 7 cycles to service
- NMI: 7 cycles to service
- Reset: Takes ~6 cycles

## PPU Timing (2C02)

### Scanline Structure
- 341 PPU dots per scanline
- 262 scanlines per frame (NTSC)
- CPU runs at 1/3 PPU clock speed

### Critical Timings
- VBLANK starts at scanline 241
- Sprite 0 hit detection timing
- VRAM access windows

### Register Access
- Some registers have specific access timing
- $2004 (OAM data) timing considerations
- $2007 (PPU data) read buffer behavior

## APU Timing (2A03)

### Frame Counter
- 4-step sequence: 3728.5, 7457, 11186, 14914.5 cycles
- 5-step sequence: 3728.5, 7457, 11186, 14914.5, 18640.5 cycles

### Sample Rate
- Approximately 44.1 kHz output
- Internal clocking varies by channel

## DMA Timing

### OAM DMA ($4014)
- Takes 513 or 514 cycles depending on CPU cycle alignment
- Suspends CPU execution
- Can conflict with DMC DMA

### DMC DMA
- Steals CPU cycles when active
- Can cause timing conflicts
- Affects audio quality if not handled properly

## Mapper-Specific Timing

### MMC3 IRQ
- Counts PPU A12 rising edges
- IRQ triggered on counter reaching 0
- Timing critical for scanline effects

## References

- Nesdev Wiki timing documentation
- Hardware test ROM results
- Cycle counting analysis
