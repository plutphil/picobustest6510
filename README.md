# 6510 bus test

This PlatformIO Arduino sketch provides a small 6510 memory monitor and test
programs. The default ROM at `$8000` executes on the 6510: it configures only
peripheral-port bit P0 as an output, alternates P0 low and high, and delays
between transitions to blink a connected LED. The delay routine is part of the
ROM at `$8040`. At startup, the sketch restores and runs a saved RAM image if
one exists; otherwise the 6510 remains in RESET until commanded. The serial
monitor runs at 115200 baud.

## Serial monitor commands

- `start` / `stop`: run the P0 blink program or hold the 6510 in reset.
- `load <addr> <hex-bytes>`: load contiguous 6510 machine code into RAM while
	stopped; bytes are given as one hexadecimal string, for example
	`load 0200 78A9018D0000`.
- `run <addr>`: reset the 6510 and execute a loaded RAM program without clearing
	RAM.
- `save <addr> <count>`: save a stopped RAM region to flash and autostart it
	after the next Pico reset. The entry point is `<addr>`.
- `clearsave`: erase the saved image and disable autostart.
- `terminal`: start the 6510 if needed and enter raw UART terminal mode. Typed
	bytes are sent to the emulated UART; press Ctrl-X to return to the monitor.
- `pintest`: run a 6510 program that writes one-hot and one-cold values while
	toggling each address bit around `$5555`. `status` reports PASS only after all
	32 expected writes match and every address/data bit has been observed both
	high and low. This checks pin transitions and the Pico's sampled bus values;
	it is not a RAM retention test or a substitute for checking shorts with a
	meter or logic analyzer.
- `peek <addr>`: read one byte; addresses and values are hexadecimal.
- `poke <addr> <byte>`: write RAM at `$0002-$07FF`.
- `dump <addr> [count]`: display a byte range, default 16 bytes.
- `irq <byte>`: queue a hexadecimal byte and assert active-low IRQ.
- `status`: show cycles, IRQ-vector reads, byte count, and test result.

## RAM program examples

Stop the CPU before loading code. The monitor accepts at most 26 bytes per
`load` command, so split longer programs into contiguous chunks. These examples
use `$0300` for code and `$0340` for a shared delay routine; `$0200-$0201` are
used by the IRQ handler.

### P0 blink

This program makes P0 an output and alternates it low/high, with a delay after
each change:

```text
stop
load 0300 78A9018D0000A9008D0100204003A9018D01002040034C0603
load 0340 A9048502A202A098EA88D0FCCAD0F7C602D0F160
run 0300
```

### Active-low P0-P5 chaser

This sequence steps from P0 through P5, driving the selected output low and the
other five high:

```text
stop
load 0300 78A93F8D0000A9018503A503493F8D0100204003A5030A8503C9
load 031A 40D0EDA90185034C0A03
load 0340 A9048502A202A098EA88D0FCCAD0F7C602D0F160
run 0300
```

To make the chaser start automatically after Pico reset, stop it and save the
code and delay routine together:

```text
stop
save 0300 0054
```

Use `clearsave` to remove this autostart program.

RAM programs are limited to `$0002-$07FF`. The saved image occupies the final
4 KB flash sector and includes a checksum; `save` is accepted only while the
6510 is stopped. Keep the firmware binary smaller than this reserved sector.
To time a RAM delay without
rebuilding, load a program that writes marker bytes to `$D001` before and after
the delay, then timestamp those bytes on the host serial port. `$0000` and
`$0001` are internal 6510 peripheral-port registers and are not reliably
observable through this external bus monitor, so use a physical probe to
measure P0 itself.

Basic echo test: enter `start`, then `irq 41`. The monitor should receive the
raw byte `A`; `status` should report `UART echo test: PASS`, RAM `$0200` should
contain `$41`, and RAM `$0201` should increment.

Interactive echo: enter `terminal`, then type characters. The emulated IRQ
handler echoes each received byte back to the console. Press Ctrl-X to leave
terminal mode and use monitor commands again.

The emulated UART is memory mapped at `$D000` (status: bit 0 set when data is
available) and `$D001` (data: reading consumes a byte and updates IRQ; writing
sends that byte to the USB serial monitor). The IRQ handler reads each byte and
writes it back to `$D001` as an echo. Unmapped reads return `$EA`.

## GPIO wiring

| 6510 signal | Raspberry Pi Pico GPIO |
| --- | ---: |
| A0-A15 | GP0-GP15 |
| D0-D7 | GP16-GP23 |
| R/W | GP24 |
| CLK input (Phi0) | GP25 |
| RDY | GP26 (Pico A0) |
| IRQ | GP27 (Pico A1) |
| NMI | GP28 (Pico A2) |
| RESET | GP29 (A3; not usable as a normal GPIO on standard Pico board) |
| GND | GND |

The Pico drives CLK, RDY, IRQ, NMI, and RESET. IRQ and NMI are held inactive
high; RDY is held high. R/W and the address bus are inputs to the Pico. The
data bus is driven with `$EA` for reads and changed to input for CPU writes.
The `irq` command drives IRQ low until the CPU acknowledges the IRQ vector;
the byte remains queued until `$D001` is read.

## Signal specification checks

- **Reset:** `start` holds RESET low for eight clock pulses, then releases it
	for the program run.
- **R/W and data bus:** R/W is sampled as an input. Pico data pins are released
	for CPU writes and driven for CPU reads, matching the bidirectional bus
	requirement.
- **AEC:** The excerpt says the address, R/W, and data buses are valid only
	while AEC is high. AEC is not included in the current pin map or firmware;
	it must be held at its active level for the bus values to be valid.
- **Clock:** The excerpt requires a two-phase non-overlapping clock or control
	of the internal generator. This sketch toggles one GPIO only; that is not a
	verified two-phase clock interface. Confirm the exact 6510 variant and
	clock-pin requirements before relying on this test.
- **NOP behavior:** `$EA` is returned for unmapped reads. The reset vector and
	test ROM are explicitly mapped, and 2 KiB RAM stores CPU writes.
- **Pico GPIO map:** GP29/A3 is not exposed as a normal user GPIO on the
	standard Pico board; it is used for VSYS measurement. Do not use it as the
	wired RESET output. Rework the pin assignment and firmware to put RESET on
	an available, exposed GPIO before connecting it.
- **Voltage:** The excerpt's TTL-level description does not make 6510 outputs
	safe for the Pico. Pico GPIO is not 5 V tolerant; level-shift CPU outputs,
	share ground, and verify that 3.3 V Pico outputs satisfy the CPU inputs.

## Electrical caution

A 6510 is normally a 5 V part; Pico GPIO is **not 5 V tolerant**. Put suitable
level shifting/buffering between all 6510 outputs and Pico inputs (A0-A15,
R/W, and D0-D7 during CPU writes). Ensure the 6510 can recognize the Pico's
3.3 V output levels, use a shared ground, and do not connect an external clock
driver to the same CLK input. GP25 also controls the Pico's onboard LED.

Build with `pio run`, upload with `pio run -t upload`, then open the serial
monitor at 115200 baud. Run `pintest` and then `status` to inspect the pin test
result. The IRQ test can be run separately with `start`, `irq 41`, and
`status`/`peek 200`.

On the current bench, the IRQ vector and handler are fetched, but the CPU also
re-enters the handler while the Pico reports its IRQ output pin HIGH. The
firmware therefore reports the test as failed if IRQ entries do not match
consumed bytes. Check the physical IRQ connection/level and clock/AEC timing
before treating the interrupt path as validated.