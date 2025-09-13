#pragma once

// This file is deprecated - APU implementation is now in apu/apu.hpp
// Kept for backward compatibility only

#include "apu/apu.hpp"

namespace nes {
// For backward compatibility, alias APU as APUStub
using APUStub = APU;
}

// TODO: Remove this file once all references are updated to use APU directly
} // namespace nes
