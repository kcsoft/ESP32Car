# ESP32 Car — Implementation Plan

A step-by-step guide to building the full ESP32 Car project. Each phase builds on the previous one, so you can test incrementally along the way.

---

## Phase 1: Project Setup & Basic Serial Output

**Goal:** Confirm the toolchain works and the ESP32 boots correctly.

- [x] **1.1** Open the project in VSCode with PlatformIO.
- [x] **1.2** Update `platformio.ini` to include the monitor speed and required library dependencies:
  ```ini
  [env:esp32dev]
  platform = espressif32
  board = esp32dev
  framework = arduino
  monitor_speed = 115200
  lib_deps =
      ESP32Async/ESPAsyncWebServer
      ESP32Async/AsyncTCP
      tzapu/WiFiManager
  ```
- [x] **1.3** Replace the placeholder `main.cpp` with a minimal sketch that prints to Serial:
  ```cpp
  #include <Arduino.h>

  void setup() {
      Serial.begin(115200);
      Serial.println("ESP32 Car booting...");
  }

  void loop() {}
  ```
- [x] **1.4** Build, upload, and verify "ESP32 Car booting..." appears in the serial monitor.

---

## Phase 2: PWM Motor Control (Wheels — BTS7960)

**Goal:** Drive the wheel motors forward and backward at variable speed.

- [x] **2.1** Define pin constants and PWM parameters:
  ```cpp
  // Wheel motor pins (BTS7960)
  #define MOTOR_FWD_PIN  18
  #define MOTOR_BWD_PIN  19

  // PWM settings
  #define PWM_FREQ       20000   // 20 kHz
  #define PWM_RESOLUTION 8       // 0-255 duty
  #define PWM_CH_FWD     0
  #define PWM_CH_BWD     1
  ```
- [x] **2.2** In `setup()`, attach each pin to its own LEDC channel:
  ```cpp
  ledcAttach(MOTOR_FWD_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_BWD_PIN, PWM_FREQ, PWM_RESOLUTION);
  ```
- [x] **2.3** Write helper functions that enforce the "only one direction active at a time" safety rule:
  ```cpp
  void setMotorForward(uint8_t speed) {
      ledcWrite(MOTOR_BWD_PIN, 0);
      ledcWrite(MOTOR_FWD_PIN, speed);
  }

  void setMotorBackward(uint8_t speed) {
      ledcWrite(MOTOR_FWD_PIN, 0);
      ledcWrite(MOTOR_BWD_PIN, speed);
  }

  void stopMotor() {
      ledcWrite(MOTOR_FWD_PIN, 0);
      ledcWrite(MOTOR_BWD_PIN, 0);
  }
  ```
- [x] **2.4** Test by calling `setMotorForward(128)` in `setup()` and confirming the motor spins. Then test backward and stop.

---

## Phase 3: PWM Steering Control (DRV8871)

**Goal:** Steer left and right using the DRV8871 driver.

- [x] **3.1** Define steering pin constants and PWM channels:
  ```cpp
  // Steering pins (DRV8871)
  #define STEER_LEFT_PIN   22
  #define STEER_RIGHT_PIN  23

  #define PWM_CH_LEFT      2
  #define PWM_CH_RIGHT     3
  ```
- [x] **3.2** Attach LEDC channels for steering in `setup()`:
  ```cpp
  ledcAttach(STEER_LEFT_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(STEER_RIGHT_PIN, PWM_FREQ, PWM_RESOLUTION);
  ```
- [x] **3.3** Write helper functions with the same one-direction-at-a-time safety rule:
  ```cpp
  void steerLeft(uint8_t intensity) {
      ledcWrite(STEER_RIGHT_PIN, 0);
      ledcWrite(STEER_LEFT_PIN, intensity);
  }

  void steerRight(uint8_t intensity) {
      ledcWrite(STEER_LEFT_PIN, 0);
      ledcWrite(STEER_RIGHT_PIN, intensity);
  }

  void steerStraight() {
      ledcWrite(STEER_LEFT_PIN, 0);
      ledcWrite(STEER_RIGHT_PIN, 0);
  }
  ```
- [x] **3.4** Test each direction manually from `setup()` and verify physical behaviour.

---

## Phase 4: Analog Foot Pedal Input

**Goal:** Read the foot pedal and map its value to motor speed.

- [x] **4.1** Define the pedal pin:
  ```cpp
  #define PEDAL_PIN  36   // ADC1_CH0, input only
  ```
- [x] **4.2** Read the pedal in `loop()` and map the 12-bit ADC value (0-4095) to the PWM duty range (0-255):
  ```cpp
  uint16_t raw = analogRead(PEDAL_PIN);
  uint8_t speed = map(raw, 0, 4095, 0, 255);
  ```
- [x] **4.3** Add a small dead-zone at the bottom (e.g., values below 50 → 0) to avoid creeping when the pedal is released.
- [x] **4.4** Print the mapped speed to Serial and verify it tracks pedal movement smoothly.

---

## Phase 5: Wi-Fi with Captive Portal

**Goal:** Connect to a configured Wi-Fi network, or fall back to an AP with a captive portal for configuration.

- [x] **5.1** Choose and add a captive portal library to `platformio.ini` `lib_deps` (e.g., [WiFiManager for ESP32](https://github.com/tzapu/WiFiManager)):
  ```ini
  lib_deps =
      ...
      tzapu/WiFiManager
  ```
- [x] **5.2** In `setup()`, initialise WiFiManager and attempt auto-connect. If it fails, it will spawn the "ESP32-Car" AP automatically:
  ```cpp
  #include <WiFiManager.h>

  WiFiManager wm;
  // Automatically tries saved credentials; falls back to AP
  bool connected = wm.autoConnect("ESP32-Car");
  if (!connected) {
      Serial.println("Failed to connect — running as AP");
  } else {
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());
  }
  ```
- [x] **5.3** Test by flashing, connecting to the "ESP32-Car" AP from your phone, configuring your home Wi-Fi via the portal, and confirming the ESP32 prints its IP.
- [ ] **5.4** Confirm that on subsequent boots it reconnects automatically without showing the portal.

---

## Phase 6: Async Web Server & Static Web Interface

**Goal:** Serve an HTML/CSS/JS control page from the ESP32.

- [x] **6.1** Create the web interface files. Store them in `data/` so they can be uploaded to SPIFFS/LittleFS:
  ```
  data/
    index.html
    style.css
    script.js
  ```
- [x] **6.2** Add filesystem configuration to `platformio.ini`:
  ```ini
  board_build.filesystem = littlefs
  ```
- [x] **6.3** Initialise LittleFS and the async web server in `setup()`:
  ```cpp
  #include <ESPAsyncWebServer.h>
  #include <LittleFS.h>

  AsyncWebServer server(80);

  LittleFS.begin();
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
  ```
- [x] **6.4** Build the `index.html` with:
  - Left / Right steering buttons (or a horizontal slider).
  - A virtual joystick (or forward / backward buttons) for drive direction.
  - A speed indicator that reflects the pedal value received over WebSocket.
  - Responsive layout (works on mobile and desktop).
- [x] **6.5** Upload the filesystem image (`pio run -t uploadfs`) and verify the page loads in a browser at the ESP32's IP.

---

## Phase 7: WebSocket Communication

**Goal:** Real-time bidirectional control between the web UI and the ESP32.

- [x] **7.1** Set up the WebSocket endpoint on the server:
  ```cpp
  AsyncWebSocket ws("/ws");

  void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
      if (type == WS_EVT_DATA) {
          // Parse incoming commands
      }
  }

  // In setup():
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  ```
- [x] **7.2** Define a simple plain-text command protocol (no JSON overhead). A single WebSocket frame may contain one or more commands separated by `;`. Commands from the UI → ESP32:
  ```
  left
  right
  center
  forward <0-255>   (e.g. forward 100)
  backward <0-255>  (e.g. backward 200)
  stop
  ping

  Multiple commands in one frame: "left;forward 200"
  ```
- [x] **7.3** Messages from ESP32 → UI (status updates). Multiple updates may be batched in one frame separated by `;`:
  ```
  speed 128
  rssi -65

  Batched: "speed 128;rssi -65"
  ```
- [x] **7.4** Parse incoming WebSocket messages in `onWsEvent` using `strtok_r` (reentrant) to correctly handle nested tokenisation — first split on `;`, then split each command on space:
  ```cpp
  char *outerSave;
  char *msg = strtok_r(buf, ";", &outerSave);
  while (msg) {
      char *innerSave;
      char *cmd = strtok_r(msg, " ", &innerSave);
      if (strcmp(cmd, "forward") == 0) {
          char *param = strtok_r(nullptr, " ", &innerSave);
          uint8_t speed = param ? (uint8_t)constrain(atoi(param), 0, 255) : 0;
          setMotorForward(speed);
      }
      // ... left / right / center / backward / stop / ping handled similarly
      msg = strtok_r(nullptr, ";", &outerSave);
  }
  ```
- [x] **7.5** In `loop()`, periodically read the pedal, update motor speed, and broadcast the current speed to all connected WebSocket clients:
  ```cpp
  void loop() {
      uint8_t speed = readPedal();
      // apply speed to current direction
      // broadcast to clients
      ws.textAll("speed " + String(speed));
      ws.cleanupClients();
      delay(50);
  }
  ```
- [x] **7.6** In `script.js`, open the WebSocket and wire UI buttons to send commands. Incoming frames are split on `;` before processing, allowing the ESP32 to batch multiple status updates in one frame:
  ```js
  const ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onmessage = (event) => {
      event.data.trim().split(';').forEach(rawMsg => {
          const parts = rawMsg.trim().split(' ');
          if (parts[0] === 'speed') {
              document.getElementById('speed-display').textContent = parts[1];
          } else if (parts[0] === 'rssi') {
              // update signal indicator
          }
      });
  };

  function send(msg) {
      ws.send(msg);
  }

  // Examples:
  send('left');              // single command
  send('left;forward 180');  // two commands in one frame
  send('stop');              // stop motors
  ```
- [x] **7.7** Test end-to-end: open the web UI, press buttons, and confirm the motors respond in real time.

---

## Phase 8: Safety & Edge Cases

**Goal:** Make the system robust and safe to operate.

- [x] **8.1** **Watchdog / heartbeat:** If no WebSocket message is received for N seconds (e.g., 2s), automatically stop the motors (client disconnect, Wi-Fi drop, etc.).
- [x] **8.2** **Graceful stop on client disconnect:** Handle `WS_EVT_DISCONNECT` to stop all motors if no other client is connected.
- [x] **8.3** **Input validation:** Validate all incoming WebSocket messages; ignore malformed data.
- [x] **8.4** **Prevent simultaneous conflicting signals:** Ensure the code never drives both FWD + BWD or LEFT + RIGHT at the same time (already handled in helpers, but add a guard in `onWsEvent` as well).
- [x] **8.5** **Boot-safe state:** Ensure all motor outputs are LOW/OFF immediately on boot before any logic runs.

---

## Phase 9.5: Motor Current Sensing (BTS7960 IS pins)

**Goal:** Read the BTS7960 R_IS / L_IS current-sense outputs and stream them to the web UI.

- [x] **9.5.1** Wire R_IS → GPIO34 and L_IS → GPIO35. These are ADC1 input-only pins, fully compatible with Wi-Fi operation. **Do not use ADC2 pins (GPIO0/2/4/12–15/25–27) — ADC2 is disabled by the Wi-Fi radio and always returns 0.**
- [x] **9.5.2** Add pin defines:
  ```cpp
  #define MOTOR_RIS_PIN  34   // BTS7960 R_IS — forward current sense (ADC1_CH6, input-only)
  #define MOTOR_LIS_PIN  35   // BTS7960 L_IS — backward current sense (ADC1_CH7, input-only)
  ```
- [x] **9.5.3** In `loop()`, read both pins every 1 s, map 12-bit ADC (0–4095) → 0–255 with a right-shift, and broadcast alongside the RSSI update:
  ```cpp
  uint8_t risCurrent = (uint8_t)(analogRead(MOTOR_RIS_PIN) >> 4);
  uint8_t lisCurrent = (uint8_t)(analogRead(MOTOR_LIS_PIN) >> 4);
  ws.textAll("rssi " + String(WiFi.RSSI()) +
             ";current_fwd " + String(risCurrent) +
             ";current_bwd " + String(lisCurrent));
  ```
- [x] **9.5.4** Extend the WebSocket protocol with two new server → client messages:
  ```
  current_fwd <0-255>   (R_IS — current while driving forward)
  current_bwd <0-255>   (L_IS — current while driving backward)
  ```
- [x] **9.5.5** Add two current-bar widgets to the web UI (one for FWD, one for BWD) and wire them to the new messages in `script.js`.

---

## Phase 9: Polish & Final Integration

**Goal:** Bring everything together and refine.

- [ ] **9.1** Refactor `main.cpp` into clean modules if desired (e.g., `motor.h`, `steering.h`, `webserver.h`).
- [ ] **9.2** Fine-tune PWM frequency — verify 20 kHz works well with both drivers; adjust if you hear audible whine or get poor response.
- [ ] **9.3** Calibrate the foot pedal dead-zone and max value with the actual hardware.
- [ ] **9.4** Style the web interface — add visual feedback (active button highlighting, speed bar/gauge).
- [ ] **9.5** Test on mobile (primary use case) — ensure touch controls are responsive and buttons are large enough.
- [ ] **9.6** Full system test: power from 12V battery, drive the car around, verify steering, speed control, Wi-Fi range, and reconnection behaviour.

---

## Quick Reference — Pin Map

| GPIO | Function                           | Driver   |
|------|------------------------------------|----------|
| 18   | Wheel motor FORWARD (PWM)          | BTS7960  |
| 19   | Wheel motor BACKWARD (PWM)         | BTS7960  |
| 34   | Wheel motor R_IS (current FWD, ADC)| BTS7960  |
| 35   | Wheel motor L_IS (current BWD, ADC)| BTS7960  |
| 22   | Steering LEFT (PWM)                | DRV8871  |
| 23   | Steering RIGHT (PWM)               | DRV8871  |
| 36   | Foot pedal (ADC input)             | —        |

---

## Dependencies Summary

| Library              | Purpose                       |
|----------------------|-------------------------------|
| ESPAsyncWebServer    | Async HTTP server & WebSocket |
| AsyncTCP             | TCP transport for above       |
| WiFiManager (tzapu)  | Captive portal & Wi-Fi config |
| LittleFS (built-in)  | Serve static web files        |
