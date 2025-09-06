# NES Mapper Implementation Notes

This document contains implementation details for various NES mappers.

## Mapper 000 (NROM)

### Memory Layout
- PRG ROM: 16KB or 32KB
- CHR ROM: 8KB
- No bank switching

### Implementation Notes
- Simplest mapper to implement
- Good starting point for testing
- No special registers

## Mapper 001 (MMC1)

### Memory Layout
- PRG ROM: Up to 512KB
- PRG RAM: Up to 32KB
- CHR ROM/RAM: Up to 128KB

### Registers
- Control register ($8000-$9FFF)
- CHR bank 0 ($A000-$BFFF)
- CHR bank 1 ($C000-$DFFF)
- PRG bank ($E000-$FFFF)

### Implementation Notes
- Serial shift register interface
- 5 writes to load a register
- Various PRG banking modes

## Mapper 002 (UxROM)

### Memory Layout
- PRG ROM: Up to 4MB
- CHR RAM: 8KB
- Fixed bank at $C000-$FFFF

### Implementation Notes
- Simple bank switching
- Write to $8000-$FFFF selects bank
- No CHR banking

## Mapper 003 (CNROM)

### Memory Layout
- PRG ROM: 16KB or 32KB
- CHR ROM: Up to 2MB

### Implementation Notes
- CHR bank switching only
- Write to $8000-$FFFF selects CHR bank
- No PRG bank switching

## Mapper 004 (MMC3)

### Memory Layout
- PRG ROM: Up to 512KB
- PRG RAM: Up to 8KB
- CHR ROM/RAM: Up to 256KB

### Registers
- Bank select ($8000-$9FFF, even)
- Bank data ($8000-$9FFF, odd)
- Mirroring ($A000-$BFFF, even)
- PRG RAM protect ($A000-$BFFF, odd)
- IRQ latch ($C000-$DFFF, even)
- IRQ reload ($C000-$DFFF, odd)
- IRQ disable ($E000-$FFFF, even)
- IRQ enable ($E000-$FFFF, odd)

### Implementation Notes
- Complex bank switching modes
- Scanline counter for IRQ
- A12 rising edge detection critical

## Implementation Strategy

1. Start with Mapper 000 (NROM)
2. Add Mapper 002 (UxROM) for simple PRG banking
3. Implement Mapper 001 (MMC1) for complex banking
4. Add Mapper 004 (MMC3) for IRQ generation
5. Expand to other mappers as needed

## Testing

- Use test ROMs for each mapper
- Verify bank switching behavior
- Test IRQ timing (MMC3)
- Check bus conflicts where applicable

## References

- Nesdev Wiki mapper documentation
- Test ROM suites
- Game compatibility lists
