# NES PPU (Picture Processing Unit) Implementation Design

## Overview
The PPU is the NES's graphics chip that generates video output. It runs at 3x the CPU clock speed (CPU: 1.79 MHz, PPU: 5.37 MHz) and produces 240 scanlines of 256 pixels each at ~60Hz.

## Integration with Existing Architecture

### File Structure (following your pattern)
```
include/ppu/
├── ppu.hpp           # Main PPU class
├── ppu_registers.hpp # Register definitions and masks
├── ppu_memory.hpp    # VRAM, OAM management
└── ppu_renderer.hpp  # Rendering pipeline

src/ppu/
├── ppu.cpp
├── ppu_registers.cpp
├── ppu_memory.cpp
└── ppu_renderer.cpp
```

### Class Structure (matching your Component pattern)
```cpp
namespace nes {
class PPU : public Component {
public:
    PPU();
    ~PPU() override = default;

    // Component interface (matching your pattern)
    void reset() override;
    void power_on() override;

    // CPU interface (memory-mapped registers)
    uint8_t read_register(uint16_t address);
    void write_register(uint16_t address, uint8_t value);

    // Main execution
    void tick();  // Called 3 times per CPU cycle

    // Frame status
    bool is_frame_ready() const;
    const uint32_t* get_frame_buffer() const;
```

## Memory-Mapped Registers ($2000-$2007, mirrored through $3FFF)

### Register Map (matching your bus address handling style)
```cpp
enum class PPURegister : uint16_t {
    PPUCTRL   = 0x2000,  // Write-only
    PPUMASK   = 0x2001,  // Write-only
    PPUSTATUS = 0x2002,  // Read-only
    OAMADDR   = 0x2003,  // Write-only
    OAMDATA   = 0x2004,  // Read/Write
    PPUSCROLL = 0x2005,  // Write-only (2x)
    PPUADDR   = 0x2006,  // Write-only (2x)
    PPUDATA   = 0x2007,  // Read/Write
};
```

### Register Bit Definitions
```cpp
// PPUCTRL ($2000)
struct PPUCtrl {
    uint8_t nametable_x : 1;        // Bit 0
    uint8_t nametable_y : 1;        // Bit 1
    uint8_t increment_mode : 1;     // Bit 2 (0: +1, 1: +32)
    uint8_t sprite_pattern : 1;     // Bit 3 (0: $0000, 1: $1000)
    uint8_t background_pattern : 1; // Bit 4 (0: $0000, 1: $1000)
    uint8_t sprite_size : 1;        // Bit 5 (0: 8x8, 1: 8x16)
    uint8_t master_slave : 1;       // Bit 6 (unused)
    uint8_t nmi_enable : 1;         // Bit 7
};

// PPUSTATUS ($2002)
struct PPUStatus {
    uint8_t unused : 5;          // Bits 0-4
    uint8_t sprite_overflow : 1; // Bit 5
    uint8_t sprite_0_hit : 1;   // Bit 6
    uint8_t vblank : 1;         // Bit 7
};
```

## Internal Memory Architecture

### PPU Address Space (16KB, $0000-$3FFF)
```
$0000-$0FFF: Pattern Table 0 (CHR ROM/RAM)
$1000-$1FFF: Pattern Table 1 (CHR ROM/RAM)
$2000-$23FF: Nametable 0
$2400-$27FF: Nametable 1
$2800-$2BFF: Nametable 2
$2C00-$2FFF: Nametable 3
$3000-$3EFF: Mirrors of $2000-$2EFF
$3F00-$3F1F: Palette RAM
$3F20-$3FFF: Mirrors of $3F00-$3F1F
```

### OAM (Object Attribute Memory) - 256 bytes
```cpp
struct Sprite {
    uint8_t y_position;      // Y position - 1
    uint8_t tile_index;      // Tile number from pattern table
    uint8_t attributes;      // Palette, flip flags, priority
    uint8_t x_position;      // X position
};
```

## Rendering Pipeline

### Scanline Timing (341 PPU cycles per scanline)
```cpp
enum class ScanlinePhase {
    VISIBLE,     // 0-239: Visible scanlines
    POST_RENDER, // 240: Post-render scanline
    VBLANK,      // 241-260: Vertical blank
    PRE_RENDER   // 261: Pre-render scanline
};
```

### Per-Scanline Operations
```cpp
class PPURenderer {
    // Cycles 0-255: Visible pixels
    void render_background_pixel(int x, int y);
    void render_sprite_pixel(int x, int y);

    // Cycle 256-340: Horizontal blank
    void fetch_sprite_data();
    void prepare_next_scanline();
};
```

## Bus Integration (matching your SystemBus pattern)

### In SystemBus
```cpp
// Add to SystemBus class
private:
    std::shared_ptr<PPU> ppu_;

public:
    void connect_ppu(std::shared_ptr<PPU> ppu) {
        ppu_ = std::move(ppu);
    }

    // In read/write methods
    if (address >= 0x2000 && address <= 0x3FFF) {
        uint16_t register_addr = 0x2000 + (address & 0x0007);
        return ppu_->read_register(register_addr);
    }
```

## State Management

### Internal State Variables
```cpp
class PPU {
private:
    // Timing
    uint16_t current_cycle_;    // 0-340
    uint16_t current_scanline_; // 0-261
    uint64_t frame_counter_;

    // Registers
    uint8_t control_register_;
    uint8_t mask_register_;
    uint8_t status_register_;

    // Internal latches
    uint16_t vram_address_;     // Current VRAM address
    uint16_t temp_vram_address_; // Temporary VRAM address
    uint8_t fine_x_scroll_;     // Fine X scroll (3 bits)
    bool write_toggle_;         // First/second write toggle

    // Memory
    std::array<uint8_t, 2048> vram_;        // 2KB VRAM
    std::array<uint8_t, 32> palette_ram_;   // 32 bytes palette
    std::array<uint8_t, 256> oam_;          // 256 bytes OAM

    // Frame buffer (256x240 pixels, 32-bit RGBA)
    std::array<uint32_t, 256 * 240> frame_buffer_;
};
```

## Key Implementation Notes

### Following Your Patterns:
1. **Component Interface**: Inherits from `Component` like CPU and other components
2. **Memory Access**: Uses the same read/write pattern as your APU/RAM stubs
3. **Bus Integration**: Connects via `shared_ptr` like other components
4. **Address Handling**: Uses your `Address` type (uint16_t) consistently

### PPU-Specific Critical Details:
1. **Register Mirroring**: $2000-$2007 repeats every 8 bytes through $3FFF
2. **PPUSTATUS Read Side Effect**: Clears vblank flag and resets write toggle
3. **PPUDATA Buffering**: Reads are buffered (except palette reads)
4. **Scroll/Address Sharing**: PPUSCROLL and PPUADDR share internal latches
5. **Sprite 0 Hit**: Must be pixel-perfect for many games to work

### Timing Synchronization with CPU:
```cpp
// In main loop or SystemBus
for (int i = 0; i < 3; ++i) {
    ppu->tick();  // PPU runs 3x per CPU cycle
}
cpu->execute_instruction();
```

### NMI Interrupt Generation:
```cpp
// In PPU::tick() when entering vblank
if (scanline == 241 && cycle == 1) {
    status_register_ |= 0x80;  // Set vblank flag
    if (control_register_ & 0x80) {  // NMI enabled?
        // Signal NMI to CPU (via bus or direct connection)
        bus_->signal_nmi();
    }
}
```

## Initial Implementation Priority:
1. **Phase 1**: Register reads/writes, basic timing
2. **Phase 2**: Background rendering (nametables, patterns)
3. **Phase 3**: Sprite rendering and sprite 0 hit
4. **Phase 4**: Scrolling and advanced features

This design follows your established patterns while providing the complete PPU functionality needed for NES emulation.
