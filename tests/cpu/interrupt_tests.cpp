#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/cpu/interrupts.hpp"
#include "../../include/memory/ram.hpp"
#include <catch2/catch_all.hpp>
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
		(void)cpu.execute_instruction(); // This should process the NMI instead of the NOP

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
		(void)cpu.execute_instruction(); // This should process the IRQ instead of the NOP

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

		// IRQ is level-triggered: the pending flag stays asserted until the
		// source is acknowledged (e.g. reading $4015).  After processing,
		// the I flag is set so the CPU won't re-enter the ISR, but the
		// line itself remains asserted.
		REQUIRE(cpu.has_pending_interrupt());

		// Explicitly acknowledge to clear the pending state
		cpu.clear_irq_line();
		REQUIRE_FALSE(cpu.has_pending_interrupt());
	}

	SECTION("IRQ is maskable when interrupts disabled") {
		cpu.set_interrupt_flag(true); // Disable interrupts
		cpu.trigger_irq();

		// IRQ should be pending but not processed
		REQUIRE(cpu.has_pending_interrupt());

		// Put a NOP instruction and try to execute
		cpu.set_program_counter(0x1000);
		bus->write(0x1000, 0xEA);		 // NOP
		(void)cpu.execute_instruction(); // Should execute NOP, not IRQ

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
		bus->write(0x1000, 0xEA);		 // NOP
		(void)cpu.execute_instruction(); // Should execute NOP since IRQ is masked

		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_program_counter() == 0x1001); // NOP executed

		cpu.set_interrupt_flag(false);	 // Enable interrupts
		(void)cpu.execute_instruction(); // Should now process IRQ

		// IRQ was processed (PC at handler), but line is still asserted (level-triggered)
		REQUIRE(cpu.get_program_counter() == 0x8200);
		cpu.clear_irq_line();
		REQUIRE_FALSE(cpu.has_pending_interrupt());
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
		(void)cpu.execute_instruction(); // This should process the reset

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

		(void)cpu.execute_instruction();
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

		(void)cpu.execute_instruction(); // Process highest priority interrupt

		// Should process reset first
		REQUIRE(cpu.get_program_counter() == 0x8100);

		// Other interrupts should still be pending
		REQUIRE(cpu.has_pending_interrupt());
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI); // Next highest
	}

	SECTION("NMI preempts IRQ") {
		cpu.trigger_irq();
		cpu.trigger_nmi();

		(void)cpu.execute_instruction(); // Process highest priority interrupt

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

		// CLI at the NMI handler, then a NOP so IRQ has one instruction in
		// which its penultimate-cycle poll can detect I=0.
		bus->write(0x8000, 0x58); // CLI at NMI handler
		bus->write(0x8001, 0xEA); // NOP — IRQ detected on its penultimate cycle

		// Queue multiple interrupts
		cpu.trigger_irq();
		cpu.trigger_nmi();
		cpu.trigger_reset();

		// Interrupts are serviced at instruction boundaries based on
		// penultimate-cycle polling.  Priority: RESET > NMI > IRQ.
		// IRQ additionally requires I=0 on the penultimate cycle.

		// 1) Process RESET (highest priority, always immediate)
		(void)cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8100);
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::NMI);

		// 2) NMI fires (detected during reset handler's penultimate cycle)
		(void)cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8000);
		REQUIRE(cpu.get_pending_interrupt() == InterruptType::IRQ);

		// 3) IRQ is masked (I=1 after NMI handler), so CPU executes CLI at $8000.
		//    On CLI's penultimate cycle the I flag is still 1, so IRQ is NOT
		//    detected yet — this is the "CLI delay" property of real 6502.
		(void)cpu.execute_instruction(); // CLI → I=0, PC=$8001
		REQUIRE(cpu.get_program_counter() == 0x8001);
		REQUIRE_FALSE(cpu.get_interrupt_flag()); // I cleared by CLI

		// 4) NOP at $8001 executes.  Now I=0, and on NOP's penultimate cycle
		//    the IRQ line is detected with I clear → IRQ queued for next boundary.
		(void)cpu.execute_instruction(); // NOP → PC=$8002
		REQUIRE(cpu.get_program_counter() == 0x8002);

		// 5) IRQ fires (detected on NOP's penultimate cycle with I=0)
		(void)cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x8200);

		// Acknowledge IRQ (level-triggered — stays pending until source clears)
		cpu.clear_irq_line();
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
		(void)cpu.execute_instruction(); // Process IRQ

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

		(void)cpu.execute_instruction(); // Execute BRK

		// Check that B flag was set in pushed status
		Byte brk_status = peek_stack(*bus, 0xFC);
		REQUIRE((brk_status & 0x10) != 0); // B flag set for BRK
	}
}

// =============================================================================
// Penultimate-cycle interrupt polling tests
// =============================================================================
// On real 6502, interrupt lines are sampled on the penultimate (second-to-last)
// cycle of each instruction.  The I flag state at that moment determines whether
// IRQ is taken — not the I flag after the instruction completes.

TEST_CASE("CLI does not allow immediate IRQ (penultimate-cycle polling)", "[cpu][interrupts][timing]") {
	// On real 6502: CLI is 2 cycles.  I is cleared on the last cycle (cycle 2).
	// On the penultimate cycle (cycle 1 = opcode fetch), I is still 1.
	// Therefore IRQ is NOT detected until the instruction AFTER CLI.
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);
	cpu.set_stack_pointer(0xFF);
	cpu.set_interrupt_flag(true); // Start with I=1

	// CLI at $0200, NOP at $0201
	bus->write(0x0200, 0x58); // CLI
	bus->write(0x0201, 0xEA); // NOP

	// Assert IRQ line before CLI runs
	cpu.trigger_irq();

	// execute_instruction #1: IRQ is pending but I=1 → prev_irq_signal_ is false
	// (because trigger_irq saw I=1 when it set the latches).
	// So CLI runs: I becomes 0, PC=$0201.
	(void)cpu.execute_instruction();
	REQUIRE(cpu.get_program_counter() == 0x0201); // CLI ran, not IRQ
	REQUIRE_FALSE(cpu.get_interrupt_flag());	  // I is now 0

	// execute_instruction #2: NOP at $0201.
	// CLI's penultimate cycle had I=1, so IRQ was NOT detected after CLI.
	// NOP's penultimate cycle (cycle 1) now has I=0 and IRQ asserted → detected.
	(void)cpu.execute_instruction();
	REQUIRE(cpu.get_program_counter() == 0x0202); // NOP ran

	// execute_instruction #3: IRQ fires (detected on NOP's penultimate cycle)
	(void)cpu.execute_instruction();
	REQUIRE(cpu.get_program_counter() == 0x8200); // IRQ handler
}

TEST_CASE("SEI allows one more IRQ through (penultimate-cycle polling)", "[cpu][interrupts][timing]") {
	// On real 6502: SEI is 2 cycles.  I is set on the last cycle (cycle 2).
	// On the penultimate cycle (cycle 1 = opcode fetch), I is still 0.
	// Therefore IRQ IS detected on SEI's penultimate cycle, and fires after SEI.
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);
	cpu.set_stack_pointer(0xFF);
	cpu.set_interrupt_flag(false); // Start with I=0

	bus->write(0x0200, 0x78); // SEI

	// Assert IRQ line — with I=0 this is immediately visible via latches
	cpu.trigger_irq();

	// execute_instruction #1: IRQ is pending + prev_irq_signal_ reflects I=0.
	// So IRQ fires before SEI has a chance to execute.
	(void)cpu.execute_instruction();
	REQUIRE(cpu.get_program_counter() == 0x8200); // IRQ handler fired

	// Verify I was set by the IRQ handler
	REQUIRE(cpu.get_interrupt_flag());
}

TEST_CASE("IRQ not re-entered after handler sets I flag", "[cpu][interrupts][timing]") {
	// After the IRQ handler sets I=1, the IRQ line is still asserted but should
	// not be re-entered because the penultimate-cycle polling sees I=1.
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	setup_interrupt_vectors(*bus);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);
	cpu.set_stack_pointer(0xFF);
	cpu.set_interrupt_flag(false); // I=0 — IRQs enabled

	bus->write(0x0200, 0xEA); // NOP at starting address
	bus->write(0x8200, 0xEA); // NOP at IRQ handler

	cpu.trigger_irq();

	// IRQ fires (prev_irq from trigger_irq with I=0)
	(void)cpu.execute_instruction();
	REQUIRE(cpu.get_program_counter() == 0x8200); // IRQ handler

	// IRQ handler set I=1.  IRQ line is still asserted, but handler's
	// penultimate cycle had I=1 → prev_irq_signal_ = false.
	// So the next instruction at the handler should run, not re-enter IRQ.
	(void)cpu.execute_instruction();			  // NOP at $8200
	REQUIRE(cpu.get_program_counter() == 0x8201); // NOP ran, no re-entry
}
