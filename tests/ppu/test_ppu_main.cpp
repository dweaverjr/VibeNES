// VibeNES - NES Emulator
// PPU Test Suite Main Entry Point
// Comprehensive testing for PPU hardware accuracy

#include "../catch2/catch_amalgamated.hpp"

// Include all PPU test files
#include "memory_mapping_tests.cpp"
#include "scrolling_tests.cpp"
#include "sprite_tests.cpp"
#include "test_ppu_2c02.cpp"
#include "timing_tests.cpp"
#include "vram_address_tests.cpp"

// The tests are automatically registered by Catch2
// This file just serves as an entry point and includes all test suites
