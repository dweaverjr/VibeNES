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

## Coding Guidelines
- Use modern C++23 features when appropriate
- Prioritize accuracy over performance shortcuts
- Write clear, self-documenting code for complex hardware behaviors
- Include cycle timing comments for hardware operations
- Use strong types and concepts for hardware registers
- Implement comprehensive logging for debugging

## Focus Areas
- Hardware timing accuracy
- Edge case handling
- Memory access patterns
- Interrupt handling
- PPU/CPU synchronization

When suggesting code, emphasize correctness and maintainability for emulation-specific challenges.
