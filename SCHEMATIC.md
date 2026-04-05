# ESP32 Car — Wiring Schematic

> **Note:** The README lists GPIO25/26/32/33 for motor/steering — the firmware uses GPIO18/19/22/23. This schematic reflects the actual firmware pin assignments.

```
  12V BATT (14Ah)
    + ─┬─────────────────────────────────────────── 12V rail
        │         ┌─────────────────────────────┐
       [Buck]     │           ESP32              │
        │ 3V3 ────┤ 3V3                      GND ├──── GND
        │         │                             │
        │         │  GPIO18 (PWM CH0) ────────────┼──────────────►RPWM─┬
        │         │  GPIO19 (PWM CH1) ────────────┼──────────────►LPWM─┤ BTS7960 ─► DRIVE MOTOR
        │         │  GPIO34 (ADC1_CH6) ◄───────────┼─[2k2]┬─ R_IS─┤ (wheels)
        │         │  GPIO35 (ADC1_CH7) ◄───────────┼─[2k2]┴─ L_IS─┘
        │         │                  └─GND      │  (IS → [2k2] → GPIO → [2k2] → GND)
        │         │                             │
        │         │  GPIO22 (PWM CH2) ────────────┼──────────────►IN1─┬
        │         │  GPIO23 (PWM CH3) ────────────┼──────────────►IN2─┤ DRV8871 ─► STEER MOTOR
        │         │                             │ 12V ─► VM─┘ (steering)
        │         │  GPIO36 (ADC1_CH0) ◄───────────┼─── PEDAL WIPER
        │         └─────────────────────────────┘
        │
   3V3 ─┴──► PEDAL + / BTS7960 VCC (logic)
   GND ─────► PEDAL − / all GND rails
```

## Pin Summary

| GPIO | Dir | Function            | PWM Ch | Driver        | Notes                           |
|------|-----|---------------------|--------|---------------|---------------------------------|
| 18   | OUT | Motor Forward       | CH0    | BTS7960 RPWM  | 20 kHz / 8-bit                  |
| 19   | OUT | Motor Backward      | CH1    | BTS7960 LPWM  | 20 kHz / 8-bit                  |
| 22   | OUT | Steer Left          | CH2    | DRV8871 IN1   | 20 kHz / 8-bit                  |
| 23   | OUT | Steer Right         | CH3    | DRV8871 IN2   | 20 kHz / 8-bit                  |
| 34   | IN  | Fwd Current Sense   | —      | BTS7960 R_IS  | ADC1_CH6, via 2×2.2 kΩ divider  |
| 35   | IN  | Bwd Current Sense   | —      | BTS7960 L_IS  | ADC1_CH7, via 2×2.2 kΩ divider  |
| 36   | IN  | Pedal Potentiometer | —      | —             | ADC1_CH0, input-only            |

## Motor Drivers

### BTS7960 — Drive (wheels)

| BTS7960 Pin | Connects To                         |
|-------------|-------------------------------------|
| RPWM        | GPIO18                              |
| LPWM        | GPIO19                              |
| R_IS        | GPIO34 via 2×2.2 kΩ voltage divider |
| L_IS        | GPIO35 via 2×2.2 kΩ voltage divider |
| VCC (logic) | 3.3 V                               |
| B+ / B−     | 12 V battery                        |
| OUT1 / OUT2 | Drive motor leads                   |

> Current sense: R_IS/L_IS are scaled to 0–3.3 V via a 2×2.2 kΩ divider, then right-shifted 4 bits (`>> 4`) to fit 8 bits and broadcast over WebSocket each second.

### DRV8871 — Steering

| DRV8871 Pin | Connects To     |
|-------------|-----------------|
| IN1         | GPIO22          |
| IN2         | GPIO23          |
| VM          | 12 V battery    |
| GND         | GND             |
| OUT1 / OUT2 | Steering motor  |

> Only one of IN1/IN2 is active at a time; the firmware clears the opposing target before setting a direction.

## Pedal Potentiometer

| Pot Pin | Connects To |
|---------|-------------|
| +       | 3.3 V       |
| −       | GND         |
| Wiper   | GPIO36      |

- Dead-band: mapped values below 50/255 are clamped to 0
- Speed value broadcast to all WebSocket clients on every change

## Software

| Component    | Detail                                       |
|--------------|----------------------------------------------|
| WiFi         | WiFiManager captive-portal AP ("ESP32-Car")  |
| Web server   | ESPAsyncWebServer, port 80                   |
| WebSocket    | `/ws` — bidirectional control               |
| Filesystem   | LittleFS (`data/` → `index.html`)            |
| WS watchdog  | 2 000 ms timeout — stops all motors         |
| Ramp rate    | Motor ±5, Steering ±10 per 10 ms tick       |
