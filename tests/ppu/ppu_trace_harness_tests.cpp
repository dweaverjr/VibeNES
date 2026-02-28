#include <catch2/catch_all.hpp>
#include "ppu_trace_harness.hpp"
#include <sstream>

using namespace nes;
using namespace nes::test;

TEST_CASE("PPUTraceHarness wires components and loads synthetic cartridge", "[ppu][trace][harness]") {
	PPUTraceHarness harness;

	REQUIRE(harness.ppu() != nullptr);
	REQUIRE(harness.bus() != nullptr);
	REQUIRE(harness.cartridge() != nullptr);
	REQUIRE(harness.is_cartridge_loaded());
}

TEST_CASE("PPUTraceHarness captures sequential dot samples", "[ppu][trace][harness]") {
	PPUTraceHarness harness;
	harness.clear_trace();

	harness.capture_dots(32);

	const auto &samples = harness.trace();
	REQUIRE(samples.size() == 32);

	for (std::size_t i = 0; i < samples.size(); ++i) {
		CAPTURE(i, samples[i].ppu_state.scanline, samples[i].ppu_state.cycle);
		REQUIRE(samples[i].sample_index == i);
	}

	REQUIRE(samples.back().frame == harness.ppu()->get_frame_count());
}

TEST_CASE("PPUTraceHarness can advance to explicit timing positions", "[ppu][trace][harness]") {
	PPUTraceHarness harness;

	harness.advance_to_position(0, 0, false);
	REQUIRE(harness.ppu()->get_current_scanline() == 0);
	REQUIRE(harness.ppu()->get_current_cycle() == 0);

	harness.capture_dots(5);
	REQUIRE(harness.trace().size() == 5);
	REQUIRE(harness.latest_sample().ppu_state.scanline == 0);
}

TEST_CASE("PPUTraceHarness detects frame boundaries", "[ppu][trace][harness]") {
	PPUTraceHarness harness;

	auto start_frame = harness.ppu()->get_frame_count();
	harness.advance_to_next_frame(false);
	auto after_frame = harness.ppu()->get_frame_count();
	REQUIRE(after_frame == start_frame + 1);

	harness.clear_trace();
	harness.advance_to_next_frame(true, 2000000);
	REQUIRE_FALSE(harness.trace().empty());
	REQUIRE(harness.trace().front().frame == after_frame);
}

TEST_CASE("PPUTraceHarness emits readable trace dumps", "[ppu][trace][harness]") {
	PPUTraceHarness harness;
	harness.clear_trace();
	harness.capture_dots(3);

	std::ostringstream oss;
	harness.dump_trace(oss);
	const auto dump = oss.str();

	REQUIRE(dump.find('#') != std::string::npos);
	REQUIRE(dump.find("status=$") != std::string::npos);
}
