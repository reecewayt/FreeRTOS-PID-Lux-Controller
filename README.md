# FreeRTOS-PID-Lux-Controller

Firmware for a MicroBlaze-based embedded system that uses a TSL2561 luminosity sensor and a PID controller to stabilize an LED's light output. The system runs on FreeRTOS and lets you tune Kp, Ki, and Kd in real time using the board switches and buttons.

TODO: this readme will need to be udpate once project setup is finalized.

## Quick overview

- Target boards: Digilent Nexys A7 (Nexys4 DDR) and RealDigital Boolean Board. The design supports both; see "Hardware notes" below for differences.
- Embedded CPU: MicroBlaze (AXI-based). Uses an AXI I2C peripheral to talk to the TSL2561 sensor, and PWM output to drive an external white LED.
- Firmware: FreeRTOS-based application that implements a PID controller and a TSL2561 driver (tsl2561.c / tsl2561.h).

## Quick start

There are two common starting flows depending on what you want to do:

1) Create the Vivado project, add IP, and generate the hardware platform (recommended for new users)

From a machine with Vivado installed and available on PATH you can run the project setup TCL script. Open a shell (bash on Windows or a supported shell on Linux) and run:

```bash
# from repo root
vivado -mode tcl -source scripts/create_project.tcl
```

What the script does:
- creates a Vivado project under `build/FreeRTOS_PID_Lux_Controller` (project name used by the script)
- adds HDL, block design, and constraints from `vivado_src`
- registers the local IP repo at `vivado_src/ip` so the included `nexys4io_3_0` IP appears in the IP catalog

After Vivado finishes you can open the project in the GUI, implement, and export the hardware platform (.xsa) for Vitis.

2) Skip Vivado project creation and use the pre-exported hardware platform (.xsa)

If you only need to do firmware development in Vitis/Vivado/Vitis IDE, you can import the exported hardware platform (.xsa) found in `hw_platform/` directly into Vitis:

 - Open Vitis -> Create Platform Project -> Import hardware description -> point to `hw_platform/<your_platform>.xsa`.

This is faster for firmware-only work because it bypasses building the Vivado project on each workstation.

## Project structure (key folders)

- `vivado_src/` — HDL, block design, constraints, and a local `ip/` folder that contains `nexys4io_3_0` IP.
- `hw_platform/` — contains exported hardware platforms (.xsa) for use in Vitis.
- `scripts/` — helper scripts; `create_project.tcl` creates the Vivado project and registers the local IP repo.
- `vitis_src/` — firmware/application sources and BSPs (when created).
- `build/` — Vivado project output goes here.

## PID controller — formula and mapping to hardware

The continuous-time PID control law is:

$$u(t) = K_p e(t) + K_i \int_0^{t} e(\tau) d\tau + K_d \frac{d}{dt} e(t)$$

Where:
- e(t) = r(t) - y(t) is the error between the setpoint r(t) (desired lux) and the measured lux y(t).
- u(t) is the controller output — in this project u is mapped to the PWM duty cycle sent to the external white LED.
- Kp, Ki, Kd are the proportional, integral, and derivative gains you can tune using switches/buttons.

Discrete-time implementation (typical for firmware):

At sample interval Δt the controller uses:

- P term: Kp * e[n]
- I term: Ki * Σ e[k] * Δt (accumulated integral, usually implemented as a running sum)
- D term: Kd * (e[n] - e[n-1]) / Δt (backward difference approximation)

Implementation notes and hardware mapping:
- Sensor: TSL2561 connected to the board via I2C (AXI IIC). The MicroBlaze reads lux via the `tsl2561` driver.
- Setpoint: maintained in firmware and adjustable by the user (buttons/switches).
- Controller: runs in firmware (FreeRTOS task). Computes u and writes the PWM duty value to the PWM output (the project routes the PWM to the Nexys4IO RGB1 Blue output to drive an external LED in the PMOD port).
- Actuator: external white LED connected to a PMOD (with current-limiting resistor) receives PWM from the board.

Tuning and behavior:
- Kp responds immediately to error. Too large Kp increases oscillation; too small Kp makes slow response.
- Ki eliminates steady-state error but can cause overshoot and wind-up. Implement integral limiting (anti-windup) in firmware.
- Kd damps the response, reacting to rate of change. Noise on measurements can amplify derivative action — consider filtering or lower Kd.

## User controls and I/O mapping

The design uses Nexys4IO and standard board I/O. The following summarizes the mappings (handout names used, with Boolean board differences noted):

- Switches (slide switches):
	- Switches[7:6] — select which PID constant is edited when buttons are pressed:
		- 01: adjust Kp
		- 10: adjust Ki
		- 11: adjust Kd
	- Switches[5:4] — multiplier for the increment/decrement step (00 = ±1, 01 = ±5, 1x = ±10)
	- Switch[3] — enable change of setpoint when buttons are pressed (1 = change setpoint)
	- Switch[2] — enable/disable derivative (D)
	- Switch[1] — enable/disable integral (I)
	- Switch[0] — enable/disable proportional (P)

- Pushbuttons:
	- BtnU — increment the selected parameter (setpoint/Kp/Ki/Kd)
	- BtnD — decrement the selected parameter
	- Note: The Boolean board has only 4 pushbuttons. The project does not use BtnC; mappings are adjusted for the Boolean board as follows: {btnC(N/A), btnu(BTN0), btnd(BTN3), btnl(BTN2), btnr(BTN1)}. Also, the Boolean board lacks btnCpuReset; the project implements it as: btnCpuReset = ~(BTN0 & BTN1)

- LEDs / PWM:
	- PWM output is routed to the Nexys4IO RGB1 Blue output (external LED on PMOD). Two RGB LEDs exist but numbering differs between boards: {RGB1(RGB0), RGB2(RGB1)} on Boolean.
	- 16 on-board LEDs and 16 switches are available for debugging and status displays.

- 7-segment display:
	- Digits[7:5] display the setpoint
	- Digits[4] blank
	- Digits[3:1] display current lux from the TSL2561
	- Digit[0] blank

## Firmware and driver notes

- Create the TSL2561 driver with these suggested functions:
	- void tsl2561_init(XIic *i2c);
	- uint16_t tsl2561_readChannel(XIic *i2c, tsl2561_channel_t channel);
	- float tsl2561_calculateLux(uint16_t ch0, uint16_t ch1);

- Recommended file layout for firmware sources:
	- `vitis_src/` — workspace where you create the Vitis platform project and application code.
	- `vitis_src/src/` — application sources (main, PID task, sensor driver).

- Vitis BSP settings: when creating the platform for the MicroBlaze, select FreeRTOS and, if needed, reduce heap size in the BSP settings:

	Board Support Package -> Modify BSP Settings -> Overview -> freeRTOS10_xilinx

## Vitis workspace setup and BSP regeneration

The `vitis_src/` directory contains example FreeRTOS applications. The platform BSP is **not tracked in git** because it's auto-generated and large. Here's how to recreate it:

### Recreate the platform BSP from .xsa

**Option 1: Command-line (XSCT)**
```bash
cd vitis_src
xsct
# In XSCT shell:
platform create -name platform -hw ../hw_platform/nexys4_hw_platform.xsa -os freertos -proc microblaze_0
platform generate
exit
```

**Option 2: Vitis IDE**
1. Open Vitis IDE
2. File → New → Platform Project
3. Platform project name: `platform`
4. Create from XSA → Browse to `hw_platform/nexys4_hw_platform.xsa` (or `nexysa7_hw_platform.xsa`)
5. Operating system: `freertos`
6. Processor: `microblaze_0`
7. Click Finish and build the platform

### What's tracked in git (vitis_src)

- ✅ Application source code (`src/*.c`, `src/*.h`, `src/lscript.ld`, etc.)
- ✅ Application configuration (`vitis-comp.json`, `app.yaml`)
- ❌ `platform/` directory (BSP - regenerate from .xsa)
- ❌ `build/` directories (compiled binaries)
- ❌ `_ide/` directories (IDE metadata, bitstreams)

### Example applications included

- `freertos_hello_world/` - Basic FreeRTOS demo with LED blinking
- `free_rtos_example/` - FreeRTOS task/queue/semaphore example with GPIO interrupts

Use these as templates for your PID controller application.

## How to run full flow (Vivado → Vitis)

1. From the repo root, generate the Vivado project using the script (see Quick start).
2. Open the generated project in Vivado, run synthesis/implementation and generate bitstream.
3. Export hardware (File → Export → Export Hardware) and include the bitstream; save the `.xsa` to `hw_platform/`.
4. Open Vitis, create the platform from the `.xsa` (see above), then create your application project.

If you only need firmware development, skip steps 1–3 and use the `.xsa` already provided in `hw_platform/`.

## Hardware hookup

- TSL2561: connect to PMOD JB (I2C lines, VCC, GND). Ensure pull-ups are present on SCL/SDA if required by the board.
- White LED: connect to PMOD JC with a suitable current-limiting resistor; PWM is driven from the board's RGB1 Blue output which is routed to the PMOD in the top-level wiring.

## Tips for testing and tuning

- Start with only P active (enable P only), increase Kp until you see a reasonable response without oscillation.
- Enable I and add small Ki to remove steady-state error — watch for integral wind-up and add limits in firmware.
- Add small Kd if the system oscillates or overshoots; if the lux measurement is noisy, filter the readings or reduce Kd.

## Deliverables & grading notes (from handout)

- Video demonstrating working project
- Short design report describing control algorithm and implementation details
- Firmware and HDL sources, including any modified top-level module and updated constraints
- Exported constraint files if changed
- Block design schematic PDF (save from IP Integrator)

## References

1. Digilent Nexys A7 Reference Manual and schematics
2. RealDigital Boolean Board Reference Manual
3. TSL2561 Datasheet

---

If you'd like, I can also add a short `scripts/README.md` that documents `create_project.tcl` outputs and exact Vivado command-line examples for Windows bash. What would you like next?

