/**
 * EV Charger Edge Node Firmware
 * Platform: ESP32
 * Communication: CAN (TWAI) - Simulation mode for no hardware
 * 
 * For Fluxton Embedded Engineer Assessment
 */

#include <Arduino.h>
#include <driver/twai.h>

// ============================================================
// CONFIGURATION
// ============================================================

#define SAMPLE_MS         100
#define CAN_TX_MS         100
#define DISPLAY_MS        3000
#define EMA_ALPHA         51

#define CAN_TX_GPIO       17      
#define CAN_RX_GPIO       16      
#define CAN_ID            0x1A0

#define VOLTAGE_MAX       4800u
#define CURRENT_MAX       900u
#define POWER_MAX         22000u

#define FAULT_OV          0x01
#define FAULT_OC          0x02
#define FAULT_OP          0x04
#define FAULT_SENSOR      0x08

// ============================================================
// SENSOR SIMULATION
// ============================================================

static uint16_t read_voltage() {
    // 0.2% simulated sensor failure rate
    if (random(500) == 0) {
        return UINT16_MAX;
    }
    
    static int16_t voltage = 3500;
    static int8_t direction = 1;
    
    voltage += direction * 5;
    if (voltage > 4900) direction = -1;
    if (voltage < 2100) direction = 1;
    
    int32_t raw = voltage + random(-20, 21);
    
    return (uint16_t)constrain(raw, 2000, 5000);
}

static uint16_t read_current() {
    // 0.2% simulated sensor failure rate
    if (random(500) == 0) {
        return UINT16_MAX;
    }
    
    static int16_t current = 600;
    static int8_t direction = 1;
    
    current += direction * 3;
    if (current > 950) direction = -1;
    if (current < 150) direction = 1;
    
    int32_t raw = current + random(-10, 11);
    
    return (uint16_t)constrain(raw, 0, 1000);
}

// ============================================================
// EMA FILTER
// ============================================================

static uint32_t ema_voltage = 3500;
static uint32_t ema_current = 600;

static uint16_t ema_filter(uint32_t &state, uint16_t raw) {
    state = ((uint32_t)EMA_ALPHA * raw + 
             (uint32_t)(256 - EMA_ALPHA) * state) >> 8;
    return (uint16_t)state;
}

// ============================================================
// CAN SIMULATION (No hardware required)
// ============================================================

static uint32_t can_frame_counter = 0;
static bool can_enabled = true;  // Always "successful" in simulation

// Simplified CAN transmit that always succeeds
static bool can_transmit(uint16_t voltage, uint16_t current, 
                          uint32_t power, uint8_t faults) {
    can_frame_counter++;
    return true;  // Always succeed in simulation
}

// ============================================================
// DISPLAY
// ============================================================

static uint32_t last_display = 0;

static void display_can_frame(uint16_t voltage, uint16_t current, 
                               uint32_t power, uint8_t faults) {
    if (millis() - last_display < DISPLAY_MS) return;
    last_display = millis();
    
    Serial.println("\n");
    Serial.println("════════════════════════════════════════════════════════════════");
    Serial.println("                    CAN FRAME TRANSMISSION                      ");
    Serial.println("════════════════════════════════════════════════════════════════");
    Serial.printf("  Frame #:     %05lu\n", can_frame_counter);
    Serial.printf("  Timestamp:   %lu ms\n", millis());
    Serial.printf("  CAN ID:      0x%03X (%d)\n", CAN_ID, CAN_ID);
    Serial.println("────────────────────────────────────────────────────────────────");
    Serial.println("  RAW 8-BYTE PAYLOAD:");
    Serial.println("  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐");
    Serial.printf("  │ 0x%02X │ 0x%02X │ 0x%02X │ 0x%02X │ 0x%02X │ 0x%02X │ 0x%02X │ 0x%02X │\n",
                 (voltage >> 8) & 0xFF, voltage & 0xFF,
                 (current >> 8) & 0xFF, current & 0xFF,
                 (power >> 16) & 0xFF, (power >> 8) & 0xFF, power & 0xFF,
                 faults);
    Serial.println("  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘");
    Serial.println("────────────────────────────────────────────────────────────────");
    Serial.println("  DECODED VALUES:");
    Serial.printf("  • Voltage:  %3u → %6.1f V\n", voltage, voltage / 10.0);
    Serial.printf("  • Current:  %3u → %6.1f A\n", current, current / 10.0);
    Serial.printf("  • Power:    %5lu → %6lu W\n", power, power);
    Serial.print("  • Faults:   0x");
    if (faults < 0x10) Serial.print("0");
    Serial.print(faults, HEX);
    Serial.print(" → ");
    if (faults == 0) {
        Serial.println("NO FAULTS");
    } else {
        if (faults & FAULT_OV) Serial.print("OVERVOLTAGE ");
        if (faults & FAULT_OC) Serial.print("OVERCURRENT ");
        if (faults & FAULT_OP) Serial.print("OVERPOWER ");
        if (faults & FAULT_SENSOR) Serial.print("SENSOR_ERROR");
        Serial.println();
    }
    Serial.println("════════════════════════════════════════════════════════════════");
}

// ============================================================
// FAULT DETECTION
// ============================================================

static uint8_t last_faults = 0;

static uint8_t check_faults(uint16_t voltage, uint16_t current, 
                            uint32_t power, bool sensor_error) {
    uint8_t faults = 0;
    
    if (voltage > VOLTAGE_MAX) faults |= FAULT_OV;
    if (current > CURRENT_MAX) faults |= FAULT_OC;
    if (power > POWER_MAX) faults |= FAULT_OP;
    if (sensor_error) faults |= FAULT_SENSOR;
    
    // Edge-triggered logging
    if (faults != last_faults) {
        if (faults & FAULT_OV && !(last_faults & FAULT_OV))
            Serial.printf("\n[FAULT] OVERVOLTAGE: %.1fV > %.1fV\n", 
                         voltage / 10.0, VOLTAGE_MAX / 10.0);
        if (faults & FAULT_OC && !(last_faults & FAULT_OC))
            Serial.printf("\n[FAULT] OVERCURRENT: %.1fA > %.1fA\n", 
                         current / 10.0, CURRENT_MAX / 10.0);
        if (faults & FAULT_OP && !(last_faults & FAULT_OP))
            Serial.printf("\n[FAULT] OVERPOWER: %luW > %luW\n", 
                         power, (unsigned long)POWER_MAX);
        if (faults & FAULT_SENSOR && !(last_faults & FAULT_SENSOR))
            Serial.println("\n[FAULT] SENSOR ERROR: Using last good value");
        
        if (faults == 0 && last_faults != 0)
            Serial.println("\n[FAULT CLEARED] System normal");
        
        last_faults = faults;
    }
    
    return faults;
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    
    // Wait for Serial Monitor to connect
    while (!Serial) {
        delay(10);
    }
    
    delay(2000);  // Wait 2 seconds after connection
    
    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════════════════════════════╗");
    Serial.println("║     EV CHARGER EDGE NODE - ESP32 FIRMWARE                    ║");
    Serial.println("║     CAN Simulation Mode - No Hardware Required               ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════╝");
    Serial.println("\n");
    Serial.println("CONFIGURATION:");
    Serial.printf("  Sample Rate:     %d ms\n", SAMPLE_MS);
    Serial.printf("  CAN TX Rate:     %d ms\n", CAN_TX_MS);
    Serial.printf("  EMA Alpha:       %d/256 = %.2f\n", EMA_ALPHA, EMA_ALPHA / 256.0);
    Serial.printf("  Voltage Limit:   %.1f V\n", VOLTAGE_MAX / 10.0);
    Serial.printf("  Current Limit:   %.1f A\n", CURRENT_MAX / 10.0);
    Serial.printf("  Power Limit:     %lu W\n", (unsigned long)POWER_MAX);
    Serial.printf("  CAN ID:          0x%03X (%d)\n", CAN_ID, CAN_ID);
    Serial.println("\n");
    Serial.println("CAN Simulation Mode: Frames shown below demonstrate protocol");
    Serial.println("SYSTEM READY - Monitoring charging session\n");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    randomSeed(analogRead(34));
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
    static uint32_t last_sample = 0;
    static uint32_t last_can = 0;
    static uint16_t voltage = 3500;
    static uint16_t current = 600;
    static uint8_t faults = 0;
    
    if (millis() - last_sample >= SAMPLE_MS) {
        last_sample = millis();
        
        bool sensor_error = false;
        
        uint16_t raw_v = read_voltage();
        if (raw_v == UINT16_MAX) {
            sensor_error = true;
            Serial.printf("[WARN] Voltage sensor failed, using last good: %.1fV\n", voltage / 10.0);
            raw_v = voltage;
        }
        
        uint16_t raw_i = read_current();
        if (raw_i == UINT16_MAX) {
            sensor_error = true;
            Serial.printf("[WARN] Current sensor failed, using last good: %.1fA\n", current / 10.0);
            raw_i = current;
        }
        
        voltage = ema_filter(ema_voltage, raw_v);
        current = ema_filter(ema_current, raw_i);
        
        uint32_t power = ((uint32_t)voltage * current) / 100;
        faults = check_faults(voltage, current, power, sensor_error);
    }
    
    if (millis() - last_can >= CAN_TX_MS) {
        last_can = millis();
        
        uint32_t power = ((uint32_t)voltage * current) / 100;
        can_transmit(voltage, current, power, faults);
        display_can_frame(voltage, current, power, faults);
    }
}