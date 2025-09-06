# VibeNES - Cycle-Accurate NES Emulator

You are helping develop a cycle-accurate Nintendo Entertainment System (NES) emulator in C++23.

## Project Goals
- **Cycle-accurate emulation**: Emulate NES hardware behavior cycle-by-cycle for perfect game compatibility
- **Modern C++**: Use C++23 features extensively (modules, ranges, concepts, etc.)
- **Clean architecture**: Modular design with separate CPU, PPU, APU, and mapper components
- **Performance**: Maintain real-time performance while achieving hardware accuracy

## Architecture Components
- **CPU (6502)**: Cycle-accurate 6502 processor emulation
- **PPU (Picture Processing Unit)**: Pixel-perfect graphics rendering
- **APU (Audio Processing Unit)**: Accurate sound synthesis
- **Mappers**: Various cartridge mapper implementations
- **Bus**: Memory mapping and component interconnection

## Communication and Clarification
- **Question unclear or contradictory requests**: If a request doesn't make technical sense or seems contradictory, ask for clarification before proceeding
- **Examples of questionable requests**:
  - Implementing features that conflict with cycle-accurate emulation goals
  - Using outdated C++ patterns when C++23 alternatives exist
  - Architecture decisions that break component modularity
- **When in doubt, ask**: Better to clarify intent than implement something incorrectly

## Coding Guidelines
- Use modern C++23 features when appropriate (concepts, ranges, modules, std::expected)
- Prioritize accuracy over performance shortcuts initially
- Write clear, self-documenting code for complex hardware behaviors
- Include cycle timing comments for hardware operations
- Use strong types and concepts for hardware registers
- Implement comprehensive logging for debugging
- Follow RAII principles and avoid raw pointers
- Use const-correctness throughout

## Technical Details to Remember

### NES Hardware Specifications
- **CPU**: 6502 running at ~1.789773 MHz (NTSC)
- **PPU**: 341 dots per scanline, 262 scanlines per frame
- **Memory**: 2KB work RAM, mirrored every 2KB up to $2000
- **PPU Memory**: 2KB VRAM, 256 bytes OAM
- **Resolution**: 256x240 pixels
- **Colors**: 64 total colors, 25 usable at once

### Critical Timing Details
- CPU executes every 3 PPU dots
- DMC DMA can steal CPU cycles
- PPU register access has specific timing windows
- Sprite 0 hit occurs on specific dot timing
- IRQ timing can be hijacked by other interrupts

### Memory Layout
```
CPU Memory Map:
$0000-$07FF: 2KB Work RAM
$0800-$1FFF: Mirrors of $0000-$07FF
$2000-$2007: PPU registers
$2008-$3FFF: Mirrors of $2000-$2007
$4000-$4017: APU and I/O registers
$4018-$401F: APU and I/O functionality that is normally disabled
$4020-$FFFF: Cartridge space (PRG ROM, PRG RAM, mapper registers)

PPU Memory Map:
$0000-$0FFF: Pattern table 0
$1000-$1FFF: Pattern table 1
$2000-$23FF: Nametable 0
$2400-$27FF: Nametable 1
$2800-$2BFF: Nametable 2
$2C00-$2FFF: Nametable 3
$3000-$3EFF: Mirrors of $2000-$2EFF
$3F00-$3F1F: Palette RAM
$3F20-$3FFF: Mirrors of $3F00-$3F1F
```

## Focus Areas
- Hardware timing accuracy (especially PPU dot timing)
- Edge case handling (page boundary crossing, DMA conflicts)
- Memory access patterns and bus conflicts
- Interrupt handling and timing
- PPU/CPU synchronization
- Proper mapper behavior and IRQ generation

## Common C++23 Features to Use
- `std::expected` for error handling in ROM loading
- Concepts for template constraints (Clockable, Readable, Writable)
- Ranges for memory operations
- `constexpr` and `consteval` for compile-time optimizations
- Strong enums and enum classes
- `std::span` for memory views
- `std::bit_cast` for type punning

## Testing Strategy
- Use nestest.nes for CPU validation
- Implement test ROMs for PPU timing
- Unit tests for individual components
- Integration tests for full system behavior
- Cycle-counting validation against known timings

## Code Examples to Follow

### Strong Types
```cpp
enum class CpuRegister : std::uint8_t { A, X, Y, SP, PC };
using Address = std::uint16_t;
using Cycle = std::chrono::duration<std::int64_t, std::ratio<1, 1'789'773>>;
```

### Concepts
```cpp
template<typename T>
concept Readable = requires(T t, Address addr) {
    { t.read(addr) } -> std::same_as<std::uint8_t>;
};
```

### Error Handling
```cpp
std::expected<Cartridge, LoadError> load_rom(const std::filesystem::path& path);
```

When suggesting code, emphasize correctness and maintainability for emulation-specific challenges. Always consider the hardware behavior first, then optimize if needed.
