# Part A – Design Document

## A1. System Overview

### Platform and Language

I chose the ESP32 microcontroller and developed the firmware in C++ using the Arduino framework.

The ESP32 was selected because it provides sufficient processing capability for real-time monitoring applications and is widely used in embedded systems development. The Arduino framework enables rapid prototyping while still allowing access to low-level functionality when required.

For this assessment, CAN communication is demonstrated through software simulation without requiring additional CAN hardware, making the project easy to reproduce and evaluate.

### Major Components of the Firmware

The firmware consists of the following modules:

* Voltage and current sensor simulation
* Signal filtering using Exponential Moving Average (EMA)
* Power calculation module
* Fault detection and monitoring
* CAN frame generation and simulation
* UART logging and diagnostics

These modules work together to simulate an EV charging session, process sensor data, detect abnormal operating conditions, and report system status.

### Assumptions

The following assumptions were made during development:

* Charging voltage operates between 200V and 500V
* Charging current operates between 0A and 100A
* Sensor values are simulated and include artificial noise
* CAN frames are generated and displayed through UART output
* No external CAN transceiver or second node is required for testing

---

## A2. Data & Signal Design

### Representation of Voltage, Current, and Power

To minimize computational overhead, all values are stored as scaled integers.

* Voltage is stored with 0.1V resolution
* Current is stored with 0.1A resolution
* Power is stored in Watts

Examples:

* 350.0V is stored as 3500
* 60.0A is stored as 600

This approach provides adequate precision while avoiding unnecessary floating-point operations in the main processing path.

### Signal Filtering

An Exponential Moving Average (EMA) filter is used to smooth voltage and current readings before further processing.

EMA was selected because it:

* Reduces sensor noise effectively
* Requires minimal memory
* Has low computational overhead
* Is commonly used in embedded systems

The filter coefficient (alpha) is configurable, allowing the trade-off between responsiveness and smoothing to be adjusted easily.

### Fault Thresholds

| Fault Condition | Threshold |
| --------------- | --------- |
| Overvoltage     | > 480V    |
| Overcurrent     | > 90A     |
| Overpower       | > 22kW    |

These thresholds were selected based on the example limits provided in the assessment and represent realistic safety limits for an EV charging system.

---

## A3. CAN Frame Design

### CAN Identifier

The firmware uses CAN ID `0x1A0` as the telemetry message identifier.

A standard 11-bit CAN identifier was chosen because it is commonly used in automotive systems and provides sufficient address space for this application.

### CAN Payload Layout

The CAN frame uses the full 8-byte payload:

| Byte | Description               |
| ---- | ------------------------- |
| 0–1  | Voltage (0.1V resolution) |
| 2–3  | Current (0.1A resolution) |
| 4–6  | Power (1W resolution)     |
| 7    | Fault Flags               |

### Bit Width and Scaling Decisions

Voltage and current are allocated 16 bits each, providing sufficient range while maintaining 0.1-unit resolution.

Power is allocated 24 bits, allowing values significantly higher than the expected operating range.

The final byte is reserved for fault flags, enabling multiple fault conditions to be represented within a single CAN frame.

This payload layout provides a good balance between precision, simplicity, and efficient use of the available 8-byte CAN payload.
