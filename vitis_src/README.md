# FreeRTOS PID Lux Controller

A real-time embedded system implementing a closed-loop PID controller for maintaining target light levels using LED brightness control. Built on MicroBlaze soft processor with FreeRTOS.

## 📋 Project Overview

This system automatically adjusts LED brightness to maintain a user-configurable light level (lux) by implementing a PID feedback control loop. The TSL2561 light sensor provides measurements, the PID algorithm computes corrections, and a hardware PWM peripheral drives the LED.

**Key Features:**
- Real-time PID control with configurable gains (Kp, Ki, Kd)
- Hardware-based PWM LED control (8-bit, 0-255)
- Integer arithmetic with output filtering for smooth transitions
- Live parameter tuning via switches and buttons
- Data logging with CSV output for analysis
- 7-segment display showing setpoint and measured lux

## 🏗️ System Architecture

### Task Structure

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Lux Manager    │────▶│  PID Compute    │────▶│  LED PWM Mgr    │
│  (Sensor Read)  │     │  (Control Loop)  │     │  (Actuator)     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               │
                               ▼
                        ┌──────────────┐
                        │ Log Manager  │
                        │(CSV + Display)│
                        └──────────────┘
                               ▲
┌─────────────────┐            │
│  User Input     │────────────┘
│  (Switches/Btns)│
└─────────────────┘
```

### Module Descriptions

#### 1. **Lux Manager** (`lux_mngr.c`)
- Periodically samples TSL2561 light sensor via I2C
- Calculates lux from broadband (CH0) and IR (CH1) channels
- Configurable sample rate (default: 100ms)
- Posts measurements to PID Compute Task

#### 2. **PID Manager** (`pid_mngr.c`)
Two independent tasks:

**a) User Input Task**
- Processes GPIO interrupt-driven input from switches and buttons
- Real-time PID parameter adjustment:
  - **SW[7:6]**: Select gain (00=none, 01=Kp, 10=Ki, 11=Kd)
  - **SW[5:4]**: Step size (00=±1, 01=±5, 10/11=±10)
  - **SW[3]**: Enable setpoint adjustment
  - **SW[2:0]**: Enable P/I/D terms individually
  - **BTN_U/BTN_D**: Increment/decrement selected parameter

**b) PID Compute Task**
- Receives lux measurements from sensor
- Executes PID algorithm with integer arithmetic
- Applies exponential moving average (EMA) filter for smooth output
- Posts PWM duty cycle (0-255) to LED controller
- Sends state data to logger

#### 3. **LED PWM Manager** (`led_pwm_mngr.c`)
- Receives PWM duty cycle commands via queue
- Controls Nexys4IO RGB LED (blue channel)
- Hardware PWM generation at 50% max duty cycle

#### 4. **Log Manager** (`log_mngr.c`)
- Receives control loop snapshots via queue
- Outputs CSV format: `timestamp,setpoint,measured,Kp,Ki,Kd,P_term,I_term,D_term,PWM`
- Updates 7-segment display:
  - **Digits 7-5**: Setpoint (0-999)
  - **Digits 3-1**: Measured lux (0-999)

#### 5. **PID Controller Core** (`pid_controller.c/.h`)
- Integer-based PID implementation
- Features:
  - Configurable gains (no floating point)
  - Integral anti-windup (clamping)
  - Output saturation (0-255 range)
  - EMA output filtering (reduces jitter)
  - Individual P/I/D term enable/disable
- Derivative calculation: `D = Kd * (error - prev_error)`

## 🔧 Hardware Requirements

- **FPGA Board**: Digilent Nexys4 DDR or Nexys A7 (bit streams are provided for both)
- **Processor**: MicroBlaze soft-core
- **Peripherals**:
  - AXI GPIO (buttons and switches)
  - AXI IIC (I2C controller)
  - Nexys4IO Custom IP (RGB LED PWM, 7-segment display)
- **Sensor**: TSL2561 Light Sensor (I2C)

## 🚀 Building & Running

### Prerequisites
- Xilinx Vitis IDE (tested with 2023.x)
- FreeRTOS included in platform
- Hardware design with required peripherals

### Build Steps
1. Open Vitis workspace in `vitis_src/`
2. Import `pid_controller` application project
3. Build the project (Ctrl+B)
4. Program the FPGA bitstream
5. Run/Debug the application

### Configuration
Edit `config.h` to enable/disable debug output:
```c
#define DEBUG_1  // Comment out to disable debug prints and enable CSV logging
```

## 📊 Data Logging

When `DEBUG_1` is undefined, the system outputs CSV data via UART:

```
Timestamp_ms,Setpoint,Measured,Kp,Ki,Kd,P_term,I_term,D_term,PWM_Output
1000,100,85,2,1,0,30,15,0,45
2000,100,92,2,1,0,16,23,0,39
...
```

Use serial terminal (115200 baud) to capture data for plotting in Python/MATLAB/Excel.

## 🎮 User Controls

### Switch Configuration
| Switches | Function | Values |
|----------|----------|---------|
| SW[7:6] | Select Gain | 00=None, 01=Kp, 10=Ki, 11=Kd |
| SW[5:4] | Step Size | 00=±1, 01=±5, 10/11=±10 |
| SW[3] | Setpoint Mode | 1=Adjust setpoint, 0=Adjust gain |
| SW[2] | Enable D Term | 1=On, 0=Off |
| SW[1] | Enable I Term | 1=On, 0=Off |
| SW[0] | Enable P Term | 1=On, 0=Off |

### Button Functions
- **BTN_U**: Increment selected parameter
- **BTN_D**: Decrement selected parameter
- **BTN_C**: (Reserved)
- **BTN_L/BTN_R**: (Reserved)

### Example: Tuning Kp
1. Set SW[7:6] = 01 (select Kp)
2. Set SW[5:4] = 01 (step size = ±5)
3. Set SW[0] = 1 (enable P term)
4. Press BTN_U to increase Kp by 5
5. Press BTN_D to decrease Kp by 5

## 🧮 PID Algorithm Details

### Standard Form
```
output = Kp*error + Ki*∫error + Kd*(error - prev_error)
```

### Integer Implementation
- All calculations use `int32_t` to prevent overflow
- Gains are direct integers (no scaling factor)
- Output filtered with EMA: `filtered = (1*new + 3*old) / 4`
- Final output clamped to [0, 255] for 8-bit PWM

### Anti-Windup
Integral term clamped to prevent runaway:
```c
integral_min = -100
integral_max = 100
```

### Output Filtering
Exponential moving average reduces jitter:
```c
PID_FILTER_SHIFT = 2  // 25% new, 75% old
```
Adjustable in `pid_controller.h` (1=faster, 3=smoother).

## 📁 Project Structure

```
vitis_src/
├── pid_controller/              # Main application
│   └── src/
│       ├── main.c               # System initialization
│       ├── config.h             # Debug configuration
│       ├── pid_controller.c/h   # PID core algorithm
│       ├── pid_mngr.c/h         # PID task management
│       ├── lux_mngr.c/h         # Sensor interface
│       ├── led_pwm_mngr.c/h     # LED actuator
│       ├── log_mngr.c/h         # Data logging & display
│       ├── tsl2561.c/h          # TSL2561 sensor driver
│       └── logging.h            # Debug macros
├── drivers/                     # Custom drivers
│   └── nexys4io/                # Nexys4IO IP driver
└── platform/                    # Hardware platform
```

## 🐛 Debugging

Enable debug output in `config.h`:
```c
#define DEBUG_1
```

Debug messages tagged by module:
```
[MAIN] System initialized
[PID_MNGR] Kp updated: 5
[LUX_MNGR] Lux: 142 (CH0: 250, CH1: 30)
[LED_PWM] PWM duty cycle: 128
```

## 📝 Known Issues & Limitations

1. **Integer Precision**: PID gains are integers, limiting tuning resolution. Consider fixed-point scaling for fractional gains.
2. **Derivative Noise**: Standard derivative amplifies sensor noise. Could implement derivative-on-measurement or filtering.
3. **PWM Range**: 8-bit PWM limits output resolution. Large errors may exceed [0,255] range before saturation.
4. **I2C Timing**: TSL2561 reads block the task. Consider interrupt-driven I2C for better real-time performance.

## 👥 Authors

- **Reece Wayt** - Portland State University
- **Marco Martinez** - Portland State University

**Course**: ECE 544 - Embedded System Design  
**Institution**: Portland State University  
**Date**: Fall 2025

## 📄 License

Educational project for ECE 544. Nexys4IO IP provided by course instructor.

## 🙏 Acknowledgments

- FreeRTOS real-time kernel
- Xilinx Vitis development tools
- TSL2561 driver adapted from Adafruit/community examples
- GitHub Copilot for code assistance
- ECE 544 course staff for Nexys4IO peripheral and project guidance
