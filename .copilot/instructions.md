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

## Current Development Status
- **CPU (6502)**: ✅ 10/10 timing accuracy achieved with 146/151 official opcodes implemented (96.7% complete)
- **Instruction Categories Completed**: Load/Store, Arithmetic, Logical, Shift/Rotate, Compare, Increment/Decrement, Branch, Jump/Subroutine, Stack, Status Flag operations
- **Remaining Instructions**: BRK (0x00), TXS (0x9A), TSX (0xBA), BIT Zero Page (0x24), BIT Absolute (0x2C)
- **Memory System**: ✅ System bus with RAM and open bus behavior implemented
- **Testing Framework**: ✅ Comprehensive test suite with Catch2, 433 assertions across 25 test cases
- **Build System**: ✅ VS Code tasks for debug/release builds

## Established Development Patterns

### Build and Testing Commands
- **VS Code Tasks** (Preferred method):
  - **Build Tests**: `Ctrl+Shift+P` → "Tasks: Run Task" → "Build Tests"
  - **Run All Tests**: `Ctrl+Shift+P` → "Tasks: Run Task" → "Run Tests"
  - **Run Tests Verbose**: `Ctrl+Shift+P` → "Tasks: Run Task" → "Run Tests Verbose"
  - **Debug Build**: `Ctrl+Shift+P` → "Tasks: Run Task" → "Debug Build"

  Note: The VS Code task system automatically includes all necessary test and source files:
  - All test files: `tests/test_main.cpp tests/catch2/catch_amalgamated.cpp tests/**/*.cpp`
  - All required source files: `src/**/*.cpp` (exclude main.cpp for tests)
  - Include paths: `-Iinclude -Itests`
  - Output: `build/debug/VibeNES_All_Tests.exe`

### CPU Implementation Best Practices
- **Manual PC management**: For multi-byte instructions, read bytes individually and increment PC explicitly
- **Avoid helper functions that auto-increment**: Use `read_byte()` + manual PC increment instead of `read_word()`
- **Page boundary detection**: Use bitwise operations `(base_address & 0xFF00) != ((base_address + offset) & 0xFF00)`
- **Explicit cycle consumption**: Call `consume_cycle()` for each hardware cycle consumed
- **Instruction structure**: Cycle 1 is opcode fetch (handled by `execute_instruction()`), subsequent cycles are operand/execution

### Testing Best Practices
- **Exact cycle counts**: Use precise cycle counts in tests (4 cycles for LDA absolute,X normal, 5 for page crossing)
- **RAM address testing**: Use addresses 0x0000-0x1FFF in tests to avoid PPU/APU bus conflicts
- **Separate instruction and data**: Don't place test data at the same address as the instruction
- **Test structure**: Separate sections for normal operation, edge cases, and boundary conditions
- **PC validation**: Check program counter after instruction execution to verify correct advancement

### Test Debugging - Memory Address Validation
When tests fail with unexpected behavior (wrong values, "unknown opcode" errors):

1. **Verify Memory Address Ranges**: First check if test addresses are within valid CPU memory ranges
    - **Work RAM**: 0x0000-0x07FF (actual RAM), 0x0800-0x1FFF (mirrored)
    - **PPU Registers**: 0x2000-0x2007 (8 registers, repeated every 8 bytes through 0x3FFF)
    - **APU and I/O Registers**: 0x4000-0x4017
    - **APU and I/O Disabled**: 0x4018-0x401F
    - **Cartridge Space**: 0x4020-0xFFFF (PRG ROM, PRG RAM, mapper registers)
    - **Note**: PPU has its own separate memory space not directly accessible by CPU

2. **Red Herring Symptoms**: "Unknown opcode" or wrong accumulator values often indicate memory mapping issues, not instruction logic problems
   - Example: Test writes 0x42 to 0x3000, but 0x3000 is open bus → returns last bus value instead of 0x42
   - This can make a working instruction appear broken

3. **Valid Test Address Ranges Only Specifically for Work RAM**:
   - **Preferred**: 0x0500-0x07FF (avoids zero page and stack conflicts)
   - **Acceptable**: 0x0800-0x1FFF (mirrored RAM)

4. **Debugging Process**:
   - If instruction tests fail mysteriously, check memory addresses first
   - Use SystemBus read/write to verify test data is stored/retrieved correctly
   - Remember: NES memory map is not linear - large gaps exist between valid regions

5. **Address Correction Examples**:
   ```cpp
   // BAD - Outside RAM range
   bus->write(0x3000, 0x42);  // Open bus - won't store value

   // GOOD - Within RAM range
   bus->write(0x0500, 0x42);  // Stored in actual RAM
   ```

### Memory System Patterns
- **Address space partitioning**: RAM (0x0000-0x1FFF), PPU (0x2000-0x3FFF), APU (0x4000-0x401F), Open bus elsewhere
- **Open bus behavior**: Return `last_bus_value_` for unmapped regions with appropriate debug messages
- **Component isolation**: Each memory component handles its own address range and mirroring

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

### CPU Instruction Implementation Pattern
```cpp
void CPU6502::LDA_absolute_X() {
    // Cycle 1: Fetch opcode (already consumed in execute_instruction)
    // Cycle 2: Fetch low byte of base address
    Byte low = read_byte(program_counter_);
    program_counter_++;

    // Cycle 3: Fetch high byte of base address
    Byte high = read_byte(program_counter_);
    program_counter_++;

    // Assemble address and calculate effective address
    Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
    Address effective_address = base_address + x_register_;

    // Cycle 4: Read from effective address
    // Page boundary crossing adds 1 cycle
    if (crosses_page_boundary(base_address, x_register_)) {
        consume_cycle(); // Additional cycle for page boundary crossing
    }

    accumulator_ = read_byte(effective_address);
    update_zero_and_negative_flags(accumulator_);
    // Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}
```

### Page Boundary Detection Helper
```cpp
bool CPU6502::crosses_page_boundary(Address base_address, Byte offset) const {
    return (base_address & 0xFF00) != ((base_address + offset) & 0xFF00);
}
```

### Test Structure Example
```cpp
TEST_CASE("CPU Instruction - LDA Absolute,X", "[cpu][instructions][addressing][timing]") {
    auto bus = std::make_unique<SystemBus>();
    auto ram = std::make_shared<Ram>();
    bus->connect_ram(ram);
    CPU6502 cpu(bus.get());

    SECTION("No page boundary crossing (4 cycles)") {
        cpu.set_program_counter(0x0100);
        cpu.set_x_register(0x10);

        // Test data in RAM range
        bus->write(0x0210, 0x42);

        // Instruction at PC
        bus->write(0x0100, 0xBD); // LDA absolute,X opcode
        bus->write(0x0101, 0x00); // Low byte
        bus->write(0x0102, 0x02); // High byte

        // Exact cycle count
        cpu.tick(cpu_cycles(4));

        REQUIRE(cpu.get_accumulator() == 0x42);
        REQUIRE(cpu.get_program_counter() == 0x0103);
    }
}
```
When suggesting code, emphasize correctness and maintainability for emulation-specific challenges. Always consider the hardware behavior first, then optimize if needed.
