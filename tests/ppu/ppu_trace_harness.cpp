#include "ppu_trace_harness.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace nes::test {

namespace {
constexpr std::size_t kDefaultSafetyGuard = 1'000'000;
}

PPUTraceHarness::PPUTraceHarness() {
	bus_ = std::make_shared<SystemBus>();
	ram_ = std::make_shared<Ram>();
	cartridge_ = TestCHRData::create_test_cartridge();
	apu_ = std::make_shared<APU>();
	cpu_ = std::make_shared<CPU6502>(bus_.get());
	ppu_ = std::make_shared<PPU>();

	if (!cartridge_ || !cartridge_->is_loaded()) {
		throw std::runtime_error("Failed to initialize synthetic test cartridge for PPUTraceHarness");
	}

	connect_components();
	reset();
}

void PPUTraceHarness::connect_components() {
	bus_->connect_ram(ram_);
	bus_->connect_cartridge(cartridge_);
	bus_->connect_apu(apu_);
	bus_->connect_cpu(cpu_);

	ppu_->connect_bus(bus_.get());
	bus_->connect_ppu(ppu_);
	ppu_->connect_cartridge(cartridge_);
	ppu_->connect_cpu(cpu_.get());
}

void PPUTraceHarness::reset() {
	trace_.clear();
	sample_counter_ = 0;

	bus_->power_on();
	cartridge_->power_on();
	apu_->power_on();
	cpu_->power_on();
	ppu_->power_on();
}

void PPUTraceHarness::clear_trace() {
	trace_.clear();
	sample_counter_ = 0;
}

bool PPUTraceHarness::is_cartridge_loaded() const noexcept {
	return cartridge_ && cartridge_->is_loaded();
}

void PPUTraceHarness::write_ppu_register(std::uint16_t address, std::uint8_t value) {
	bus_->write(address, value);
}

std::uint8_t PPUTraceHarness::read_ppu_register(std::uint16_t address) {
	return bus_->read(address);
}

void PPUTraceHarness::set_vram_address(std::uint16_t address) {
	write_ppu_register(0x2006, static_cast<std::uint8_t>(address >> 8));
	write_ppu_register(0x2006, static_cast<std::uint8_t>(address & 0xFF));
}

void PPUTraceHarness::write_vram(std::uint16_t address, std::uint8_t value) {
	set_vram_address(address);
	write_ppu_register(0x2007, value);
}

std::uint8_t PPUTraceHarness::read_vram(std::uint16_t address) {
	set_vram_address(address);
	// Perform dummy read for non-palette addresses to emulate hardware behavior
	[[maybe_unused]] auto dummy = read_ppu_register(0x2007);
	return read_ppu_register(0x2007);
}

void PPUTraceHarness::write_palette(std::uint16_t address, std::uint8_t value) {
	set_vram_address(address);
	write_ppu_register(0x2007, value);
}

void PPUTraceHarness::set_scroll(std::uint8_t x, std::uint8_t y) {
	write_ppu_register(0x2005, x);
	write_ppu_register(0x2005, y);
}

void PPUTraceHarness::run_dots(std::size_t dots) {
	for (std::size_t i = 0; i < dots; ++i) {
		tick_internal(false);
	}
}

void PPUTraceHarness::capture_dots(std::size_t dots) {
	for (std::size_t i = 0; i < dots; ++i) {
		tick_internal(true);
	}
}

void PPUTraceHarness::advance_to_position(std::uint16_t target_scanline, std::uint16_t target_cycle, bool capture,
										  std::size_t safety_guard) {
	const std::size_t guard = safety_guard == 0 ? kDefaultSafetyGuard : safety_guard;
	std::size_t iterations = 0;

	while (iterations < guard) {
		if (ppu_->get_current_scanline() == target_scanline && ppu_->get_current_cycle() == target_cycle) {
			return;
		}

		tick_internal(capture);
		++iterations;
	}

	throw std::runtime_error("advance_to_position exceeded safety guard");
}

void PPUTraceHarness::advance_to_next_frame(bool capture, std::size_t safety_guard) {
	const std::size_t guard = safety_guard == 0 ? kDefaultSafetyGuard : safety_guard;
	const auto start_frame = ppu_->get_frame_count();
	std::size_t iterations = 0;

	while (iterations < guard && ppu_->get_frame_count() == start_frame) {
		tick_internal(capture);
		++iterations;
	}

	if (ppu_->get_frame_count() == start_frame) {
		throw std::runtime_error("advance_to_next_frame exceeded safety guard");
	}
}

void PPUTraceHarness::dump_trace(std::ostream &os, std::size_t max_samples) const {
	if (trace_.empty()) {
		os << "<trace empty>\n";
		return;
	}

	const std::size_t limit = std::min(max_samples, trace_.size());
	for (std::size_t i = 0; i < limit; ++i) {
		os << format_sample(trace_[i]) << '\n';
	}

	if (limit < trace_.size()) {
		os << "... (" << (trace_.size() - limit) << " more samples)\n";
	}
}

std::string PPUTraceHarness::format_sample(const TraceSample &sample) const {
	std::ostringstream ss;
	ss << '#' << sample.sample_index << " f=" << sample.frame << " sl=" << sample.ppu_state.scanline
	   << " cy=" << sample.ppu_state.cycle << std::hex << std::uppercase << std::setfill('0') << " v=$" << std::setw(4)
	   << sample.ppu_state.vram_address << " t=$" << std::setw(2) << static_cast<int>(sample.ppu_state.next_tile_id)
	   << " attr=$" << std::setw(2) << static_cast<int>(sample.ppu_state.next_tile_attribute) << " status=$"
	   << std::setw(2) << static_cast<int>(sample.status_register) << std::dec << std::nouppercase
	   << " s0=" << (sample.sprite_0_hit ? 1 : 0) << " ov=" << (sample.sprite_overflow ? 1 : 0)
	   << " fr=" << (sample.frame_ready ? 1 : 0);
	return ss.str();
}

void PPUTraceHarness::record_sample() {
	TraceSample sample{};
	sample.sample_index = sample_counter_++;
	sample.frame = ppu_->get_frame_count();
	sample.ppu_state = ppu_->get_debug_state();
	sample.status_register = ppu_->get_status_register();
	sample.mask_register = ppu_->get_mask_register();
	sample.control_register = ppu_->get_control_register();
	sample.sprite_0_hit = (sample.status_register & 0x40) != 0;
	sample.sprite_overflow = (sample.status_register & 0x20) != 0;
	sample.frame_ready = ppu_->is_frame_ready();

	trace_.push_back(sample);
}

void PPUTraceHarness::tick_internal(bool capture) {
	ppu_->tick_single_dot();
	if (capture) {
		record_sample();
	}
}

} // namespace nes::test
