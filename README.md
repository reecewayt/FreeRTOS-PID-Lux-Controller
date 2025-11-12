# FreeRTOS-PID-Lux-Controller

Firmware for a MicroBlaze-based embedded system that uses a TSL2561 luminosity sensor and a PID controller to stabilize an LED's light output. The system runs on FreeRTOS and lets you tune Kp, Ki, and Kd in real time using the board switches and buttons.



## Quick overview

- **Target boards:** Digilent Nexys A7 and Nexys 4 DDR. Both boards use the same Artix-7 FPGA (xc7a100tcsg324-1) and share nearly identical peripheral configurations. The primary difference is pin assignments for certain peripherals, which require board-specific constraint files (see Hardware Notes below).
- **Embedded CPU:** MicroBlaze (AXI-based). Uses an AXI I2C peripheral to communicate with the TSL2561 sensor, and PWM output to drive an external white LED.
- **Firmware:** FreeRTOS-based application that implements a PID controller and a TSL2561 driver (tsl2561.c / tsl2561.h).

> **Hardware Notes:** The Nexys A7 and Nexys 4 DDR boards are functionally equivalent for this project, but use different pin assignments for peripheral connections in the top-level module. Board-specific constraint files are provided in `vivado_src/constraints/`:
> - `nexys4fpga.xdc` - for Nexys 4 DDR
> - `nexysA7fpga.xdc` - for Nexys A7
> 
> The `create_project.tcl` script adds both constraint files to the project but enables `nexys4fpga.xdc` by default. **Before synthesis, ensure the correct constraint file is enabled for your target board** by opening the project in Vivado GUI and toggling the constraint files in the Sources panel (right-click → Enable/Disable). 

## Quick start

### For hardware development (Vivado)

If you need to modify the hardware design, create the Vivado project using the provided TCL script. From a machine with Vivado installed, open vivado and use the tcl console to navigate to the scripts directory `scripts/create_project.tcl`. 
Run the command: 
```bash
$ source create_project.tcl
```
If all goes well vivado will then load the created project. 

What the script does:
- Creates a Vivado project under `build/FreeRTOS_PID_Lux_Controller`
- Adds HDL, block design, and constraints from `vivado_src/`
- Registers the local IP repository at `vivado_src/ip/` so the included `nexys4io_3_0` IP appears in the IP catalog

After the script completes, open the project in Vivado GUI, run synthesis/implementation, generate the bitstream, and export the hardware platform (`.xsa`) to `hw_platform/`.

### For firmware development (Vitis)

If you're only doing firmware development, use the pre-exported hardware platform (`.xsa`) files already included in `hw_platform/`. Follow these steps to set up your Vitis workspace:

**Step 1: Open Vitis and set workspace**
- Launch Vitis IDE
- Set workspace to `vitis_src/` directory in this repository
- You'll see existing applications in the workspace, but the hardware platform needs to be regenerated from the provided `.xsa` files

**Step 2: Create the hardware platform component**
- Go to **File → New Component → Platform**
- Component name: `platform`
- Component location: `vitis_src/` directory (should already be selected)
- Click **Next**
- Under "Hardware Design", click **Browse**
  - Navigate to `hw_platform/nexys4_hw_platform.xsa` (for Nexys4 DDR) **or** `hw_platform/nexysa7_hw_platform.xsa` (for Nexys A7)
  - Select the appropriate `.xsa` file for your board
- Click **Next**
- Verify:
  - Processor: `microblaze_0`
  - Operating System: `freertos`
- Click **Finish**
- Build the platform component (this generates the BSP and may take a few minutes)

**Step 3: Build and run applications**
- The application source code is included in the workspace (`freertos_hello_world/`, `free_rtos_example/`)
- Once the platform build completes, you should be able to build and run the applications on your hardware
- Right-click on an application → **Build Project**
- Connect your board, program the FPGA, and run the application

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

The design uses Nexys4IO and standard board I/O. The following summarizes the mappings for both Nexys A7 and Nexys 4 DDR boards:

- **Switches (slide switches):**
	- Switches[7:6] — select which PID constant is edited when buttons are pressed:
		- 01: adjust Kp
		- 10: adjust Ki
		- 11: adjust Kd
	- Switches[5:4] — multiplier for the increment/decrement step (00 = ±1, 01 = ±5, 1x = ±10)
	- Switch[3] — enable change of setpoint when buttons are pressed (1 = change setpoint)
	- Switch[2] — enable/disable derivative (D)
	- Switch[1] — enable/disable integral (I)
	- Switch[0] — enable/disable proportional (P)

- **Pushbuttons:**
	- BtnU — increment the selected parameter (setpoint/Kp/Ki/Kd)
	- BtnD — decrement the selected parameter
	- BtnL, BtnR — available for additional functionality (optional)
	- BtnC — not used in this design

- **LEDs / PWM:**
	- PWM output is routed to the Nexys4IO RGB1 Blue output, then connected to an external LED on PMOD JC
	- Both boards have 2 RGB LEDs and 16 standard LEDs available for debugging and status displays

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

- Vitis BSP settings: If needed, you can adjust FreeRTOS configuration (heap size, stack size, etc.) after creating the platform:
  - In Vitis, expand the platform project
  - Navigate to: **Board Support Package → Modify BSP Settings → Overview → freeRTOS10_xilinx**
  - Adjust settings as needed and rebuild the platform

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
2. Digilent Nexys 4 DDR Reference Manual and schematics
3. TSL2561 Datasheet
4. Xilinx MicroBlaze Processor Reference Guide
5. FreeRTOS Documentation

