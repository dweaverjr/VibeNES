#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/cpu/interrupts.hpp"
#include "../../include/memory/ram.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

namespace {

/// Helper function to set up a bus with controlled memory for interrupt testing
void setup_interrupt_vectors(SystemBus &bus) {
	// Set up interrupt vectors with known test values
	// NMI Vector (0xFFFA-0xFFFB): points to 0x8000
	bus.write(0xFFFA, 0x00); // Low byte
	bus.write(0xFFFB, 0x80); // High byte

	// Reset Vector (0xFFFC-0xFFFD): points to 0x8100
	bus.write(0xFFFC, 0x00); // Low byte
	bus.write(0xFFFD, 0x81); // High byte

	// IRQ Vector (0xFFFE-0xFFFF): points to 0x8200
	bus.write(0xFFFE, 0x00); // Low byte
	bus.write(0xFFFF, 0x82); // High byte
}

/// Get the value at the top of the stack (for testing stack operations)
Byte peek_stack(const SystemBus &bus, Byte stack_pointer) {
	return bus.read(0x0100 + stack_pointer + 1);
}

} // anonymous namespace

TEST_CASE("InterruptState functionality", "[cpu][interrupts]") {
	InterruptState state;

	SECTION("Initial state") {
		REQUIRE(state.get_pending_interrupt() == InterruptType::NONE);
		REQUIRE_FALSE(state.nmi_pending);
		REQUIRE_FALSE(state.irq_pending);
		REQUIRE_FALSE(state.reset_pending);
	}

	SECTION("Setting individual interrupts") {
		state.nmi_pending = true;
		REQUIRE(state.get_pending_interrupt() == InterruptType::NMI);

		state.irq_pending = true;
		REQUIRE(state.get_pending_interrupt() == InterruptType::NMI); // NMI has higher priority

		state.reset_pending = true;
		REQUIRE(state.get_pending_interrupt() == InterruptType::RESET); // Reset has highest priority
	}

	SECTION("Interrupt priority order") {
		// Reset has highest priority
		state.reset_pending = true;
		state.nmi_pending = true;
		state.irq_pending = true;
		REQUIRE(state.get_pending_interrupt() == InterruptType::RESET);

		// NMI has second priority
		state.reset_pending = false;
		REQUIRE(state.get_pending_interrupt() == InterruptType::NMI);

		// IRQ has lowest priority
		state.nmi_pending = false;
		REQUIRE(state.get_pending_interrupt() == InterruptType::IRQ);
	}

	SECTION("Clearing interrupts") {
		state.nmi_pending = true;
		state.irq_pending = true;
		state.reset_pending = true;

		state.clear_interrupt(InterruptType::RESET);
		REQUIRE_FALSE(state.reset_pending);
		REQUIRE(state.get_pending_interrupt() == InterruptType::NMI);

		state.clear_interrupt(InterruptType::NMI);
		REQUIRE_FALSE(state.nmi_pending);
		REQUIRE(state.get_pending_interrupt() == InterruptType::IRQ);

		state.clear_interrupt(InterruptType::IRQ);
		REQUIRE_FALSE(state.irq_pending);
		REQUIRE(state.get_pending_interrupt() == InterruptType::NONE);
	}

	SECTION("Clear all interrupts") {
		state.nmi_pending = true;
		state.irq_pending = true;
		state.reset_pending = true;

		state.clear_all();
		REQUIRE_FALSE(state.nmi_pending);
		REQUIRE_FALSE(state.irq_pending);
		REQUIRE_FALSE(state.reset_pending);
		REQUIRE(state.get_pending_interrupt() == InterruptType::NONE);
	}
}

TEST_CASE("CPU interrupt triggering", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("Trigger NMI") {
		REQUIRE_FALSE(cpu.has_pending_interrupt());

		cpu.trigger_nmi();

		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI);
	}

	SECTION("Trigger IRQ") {
		REQUIRE_FALSE(cpu.has_pending_interrupt());

		cpu.trigger_irq();

		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::IRQ);
	}

	SECTION("Trigger reset") {
		REQUIRE_FALSE(cpu.has_pending_interrupt());

		cpu.trigger_reset();

		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::RESET);
	}

	SECTION("Multiple interrupts respect priority") {
		cpu.trigger_irq();
		cpu.trigger_nmi();
		cpu.trigger_reset();

		REQUIRE(cpu.get_pending_interrupt() == InterruptType::RESET);
	}
}

TEST_CASE("NMI interrupt handling", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("NMI execution sequence") {
		// Set up initial CPU state
		cpu.set_program_counter(0x1234);
		cpu.set_stack_pointer(0xFF);

		// Set up status register with known state
		cpu.set_carry_flag(true);
		cpu.set_zero_flag(false);
		cpu.set_interrupt_flag(false);
		cpu.set_decimal_flag(true);
		cpu.set_break_flag(false);
		cpu.set_overflow_flag(true);
		cpu.set_negative_flag(false);

		// Put a NOP instruction at the current PC to avoid issues
		bus->write(0x1234, 0xEA); // NOP

		// Trigger NMI and process
		cpu.trigger_nmi();
		cpu.execute_instruction(); // This should process the NMI instead of the NOP

		// Verify PC jumped to NMI vector
		REQUIRE(cpu.get_program_counter() == 0x8000);

		// Verify interrupt flag is set
		REQUIRE(cpu.get_interrupt_flag());

		// Verify stack operations (PC and status pushed)
		REQUIRE(cpu.get_stack_pointer() == 0xFC); // 3 bytes pushed

		// Check that PC was pushed to stack (high byte first)
		REQUIRE(peek_stack(*bus, 0xFE) == 0x12); // High byte of PC
		REQUIRE(peek_stack(*bus, 0xFD) == 0x34); // Low byte of PC

		// Check that status was pushed (with B flag clear, unused flag set)
		Byte pushed_status = peek_stack(*bus, 0xFC);
		REQUIRE((pushed_status & 0x10) == 0); // B flag should be clear
		REQUIRE((pushed_status & 0x20) != 0); // Unused flag should be set
		REQUIRE((pushed_status & 0x01) != 0); // Carry flag preserved
		REQUIRE((pushed_status & 0x08) != 0); // Decimal flag preserved
		REQUIRE((pushed_status & 0x40) != 0); // Overflow flag preserved

		// Verify NMI is no longer pending
		REQUIRE_FALSE(cpu.has_pending_interrupt());
	}

	SECTION("NMI is non-maskable") {
		cpu.set_interrupt_flag(true); // Set interrupt disable flag
		cpu.trigger_nmi();

		// NMI should still be processed despite interrupt flag
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI);
	}
}

TEST_CASE("IRQ interrupt handling", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("IRQ execution sequence") {
		// Set up initial CPU state
		cpu.set_program_counter(0x5678);
		cpu.set_stack_pointer(0xFF);
		cpu.set_interrupt_flag(false); // Enable interrupts

		// Set up status register
		cpu.set_carry_flag(false);
		cpu.set_zero_flag(true);
		cpu.set_decimal_flag(false);
		cpu.set_break_flag(true); // This should be cleared when pushed
		cpu.set_overflow_flag(false);
		cpu.set_negative_flag(true);

		// Put a NOP instruction at the current PC
		bus->write(0x5678, 0xEA); // NOP

		// Trigger IRQ and process
		cpu.trigger_irq();
		cpu.execute_instruction(); // This should process the IRQ instead of the NOP

		// Verify PC jumped to IRQ vector
		REQUIRE(cpu.get_program_counter() == 0x8200);

		// Verify interrupt flag is set
		REQUIRE(cpu.get_interrupt_flag());

		// Verify stack operations
		REQUIRE(cpu.get_stack_pointer() == 0xFC); // 3 bytes pushed

		// Check that PC was pushed to stack
		REQUIRE(peek_stack(*bus, 0xFE) == 0x56); // High byte of PC
		REQUIRE(peek_stack(*bus, 0xFD) == 0x78); // Low byte of PC

		// Check that status was pushed (with B flag clear)
		Byte pushed_status = peek_stack(*bus, 0xFC);
		REQUIRE((pushed_status & 0x10) == 0); // B flag should be clear
		REQUIRE((pushed_status & 0x20) != 0); // Unused flag should be set
		REQUIRE((pushed_status & 0x02) != 0); // Zero flag preserved
		REQUIRE((pushed_status & 0x80) != 0); // Negative flag preserved

		// Verify IRQ is no longer pending
		REQUIRE_FALSE(cpu.has_pending_interrupt());
	}

	SECTION("IRQ is maskable when interrupts disabled") {
		cpu.set_interrupt_flag(true); // Disable interrupts
		cpu.trigger_irq();

		// IRQ should be pending but not processed
		REQUIRE(cpu.has_pending_interrupt());

		// Put a NOP instruction and try to execute
		cpu.set_program_counter(0x1000);
		bus->write(0x1000, 0xEA);  // NOP
		cpu.execute_instruction(); // Should execute NOP, not IRQ

		// Should have executed NOP (PC incremented)
		REQUIRE(cpu.get_program_counter() == 0x1001);
		// IRQ should still be pending because interrupts are disabled
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::IRQ);
	}

	SECTION("IRQ processes when interrupts enabled") {
		cpu.set_interrupt_flag(true); // Initially disabled
		cpu.trigger_irq();

		// Put a NOP instruction
		cpu.set_program_counter(0x1000);
		bus->write(0x1000, 0xEA);  // NOP
		cpu.execute_instruction(); // Should execute NOP since IRQ is masked

		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_program_counter() == 0x1001); // NOP executed

		cpu.set_interrupt_flag(false); // Enable interrupts
		cpu.execute_instruction();	   // Should now process IRQ

		REQUIRE_FALSE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_program_counter() == 0x8200);
	}
}

TEST_CASE("Reset interrupt handling", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("Reset execution sequence") {
		// Set up initial CPU state
		cpu.set_program_counter(0x9999);
		cpu.set_stack_pointer(0x80);
		cpu.set_accumulator(0xAA);
		cpu.set_x_register(0xBB);
		cpu.set_y_register(0xCC);

		// Set all status flags
		cpu.set_carry_flag(true);
		cpu.set_zero_flag(true);
		cpu.set_interrupt_flag(false);
		cpu.set_decimal_flag(true);
		cpu.set_break_flag(true);
		cpu.set_overflow_flag(true);
		cpu.set_negative_flag(true);

		// Trigger reset and process
		cpu.trigger_reset();
		cpu.execute_instruction(); // This should process the reset

		// Verify PC jumped to reset vector
		REQUIRE(cpu.get_program_counter() == 0x8100);

		// Verify interrupt flag is set
		REQUIRE(cpu.get_interrupt_flag());

		// Verify decimal flag is cleared (6502 behavior)
		REQUIRE_FALSE(cpu.get_decimal_flag());

		// Stack pointer should be decremented by 3 (simulating stack pushes)
		REQUIRE(cpu.get_stack_pointer() == 0x7D);

		// Registers should be unchanged by reset (only flags and PC change)
		REQUIRE(cpu.get_accumulator() == 0xAA);
		REQUIRE(cpu.get_x_register() == 0xBB);
		REQUIRE(cpu.get_y_register() == 0xCC);

		// Verify reset is no longer pending
		REQUIRE_FALSE(cpu.has_pending_interrupt());
	}

	SECTION("Reset is non-maskable") {
		cpu.set_interrupt_flag(true); // Set interrupt disable flag
		cpu.trigger_reset();

		// Reset should still be processed despite interrupt flag
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::RESET);

		cpu.execute_instruction();
		REQUIRE_FALSE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_program_counter() == 0x8100);
	}
}

TEST_CASE("Interrupt priority and precedence", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("Reset preempts all other interrupts") {
		cpu.trigger_irq();
		cpu.trigger_nmi();
		cpu.trigger_reset();

		cpu.execute_instruction(); // Process highest priority interrupt

		// Should process reset first
		REQUIRE(cpu.get_program_counter() == 0x8100);

		// Other interrupts should still be pending
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI); // Next highest
	}

	SECTION("NMI preempts IRQ") {
		cpu.trigger_irq();
		cpu.trigger_nmi();

		cpu.execute_instruction(); // Process highest priority interrupt

		// Should process NMI first
		REQUIRE(cpu.get_program_counter() == 0x8000);

		// IRQ should still be pending
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::IRQ);
	}

	SECTION("Sequential interrupt processing") {
		cpu.set_program_counter(0x4000);
		cpu.set_stack_pointer(0xFF);
		cpu.set_interrupt_flag(false);

		// Queue multiple interrupts
		cpu.trigger_irq();
		cpu.trigger_nmi();
		cpu.trigger_reset();

		// Process reset first
		cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8100);
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI);

		// Process NMI second
		cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8000);
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::IRQ);

		// Process IRQ last
		cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8200);
		REQUIRE_FALSE(cpu.has_pending_interrupt());
	}
}

TEST_CASE("Interrupt vector constants", "[cpu][interrupts]") {
	SECTION("Vector addresses are correct") {
		REQUIRE(NMI_VECTOR == 0xFFFA);
		REQUIRE(RESET_VECTOR == 0xFFFC);
		REQUIRE(IRQ_VECTOR == 0xFFFE);
	}

	SECTION("Vectors are properly ordered") {
		// Ensure vectors don't overlap and are in expected order
		REQUIRE(NMI_VECTOR < RESET_VECTOR);
		REQUIRE(RESET_VECTOR < IRQ_VECTOR);
		REQUIRE((IRQ_VECTOR - NMI_VECTOR) == 4); // 2 bytes per vector
	}
}

TEST_CASE("BRK instruction vs IRQ handling", "[cpu][interrupts]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());

	SECTION("IRQ handler receives different B flag than BRK") {
		// Set up for IRQ
		cpu.set_program_counter(0x3000);
		cpu.set_stack_pointer(0xFF);
		cpu.set_interrupt_flag(false);
		cpu.set_break_flag(true); // Should be cleared for IRQ

		// Put a NOP at PC so IRQ can be processed
		bus->write(0x3000, 0xEA); // NOP

		cpu.trigger_irq();
		cpu.execute_instruction(); // Process IRQ

		// Check that B flag was cleared in pushed status
		Byte irq_status = peek_stack(*bus, 0xFC);
		REQUIRE((irq_status & 0x10) == 0); // B flag clear for IRQ

		// Reset for BRK test
		cpu.set_program_counter(0x4000);
		cpu.set_stack_pointer(0xFF);
		cpu.set_break_flag(false); // Will be set by BRK

		// Set up BRK instruction
		bus->write(0x4000, 0x00); // BRK
		bus->write(0x4001, 0x00); // BRK padding byte

		cpu.execute_instruction(); // Execute BRK

		// Check that B flag was set in pushed status
		Byte brk_status = peek_stack(*bus, 0xFC);
		REQUIRE((brk_status & 0x10) != 0); // B flag set for BRK
	}
}
