#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ── Pin & PWM constants ────────────────────────────────────────
#define MOTOR_FWD_PIN   18
#define MOTOR_BWD_PIN   19
#define STEER_LEFT_PIN  22
#define STEER_RIGHT_PIN 23
#define PEDAL_PIN       36
#define MOTOR_RIS_PIN   34   // BTS7960 R_IS — forward current sense (ADC1_CH6, input-only)
#define MOTOR_LIS_PIN   35   // BTS7960 L_IS — backward current sense (ADC1_CH7, input-only)
// ON/OFF PEDAL
#define ONOFF_PEDAL_FW  14  // GPIO for on/off pedal
#define ONOFF_PEDAL_BW  12  // GPIO for on/off pedal (backward)

#define PWM_FREQ        20000
#define PWM_RESOLUTION  8
#define PWM_CH_FWD      0
#define PWM_CH_BWD      1
#define PWM_CH_LEFT     2
#define PWM_CH_RIGHT    3

#define WS_WATCHDOG_MS  2000

// ── Ramp-rate constants (PWM steps per loop tick, i.e. per 50 ms) ─────────────
#define RAMP_STEP_FWD    2   // forward motor ramp speed
#define RAMP_STEP_BWD    2   // backward motor ramp speed
#define RAMP_STEP_STEER  10  // steering ramp speed (left & right)

// ── Server & WebSocket ─────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static unsigned long lastWsMsgMs = 0;  // 0 → watchdog fires until first client msg
static bool isBlocked = false;         // true while pedal is suppressed (WS commanded fwd/bwd, or block switch on)

// ── PWM target & current state ─────────────────────────────────
static uint8_t targetFwd   = 0, currentFwd   = 0;
static uint8_t targetBwd   = 0, currentBwd   = 0;
static uint8_t targetLeft  = 0, currentLeft  = 0;
static uint8_t targetRight = 0, currentRight = 0;

// Advance `cur` one ramp step toward `target`, write the channel, return new cur.
static uint8_t rampChannel(uint8_t cur, uint8_t target, uint8_t step, uint8_t ch) {
  if (cur < target) {
    cur = (uint8_t)min((int)cur + step, (int)target);
  } else if (cur > target) {
    cur = (uint8_t)max((int)cur - step, (int)target);
  }
  ledcWrite(ch, cur);
  return cur;
}

// ── Motor helpers ──────────────────────────────────────────────
void setMotorForward(uint8_t speed) {
  targetBwd = 0;
  targetFwd = speed;
}

void setMotorBackward(uint8_t speed) {
  targetFwd = 0;
  targetBwd = speed;
}

void stopMotor() {
  targetFwd = 0;
  targetBwd = 0;
}

// ── Steering helpers ───────────────────────────────────────────
void steerLeft(uint8_t intensity) {
  targetRight = 0;
  targetLeft  = intensity;
}

void steerRight(uint8_t intensity) {
  targetLeft  = 0;
  targetRight = intensity;
}

void steerStraight() {
  targetLeft  = 0;
  targetRight = 0;
}

// ── Pedal reading ──────────────────────────────────────────────
uint8_t readPedal() {
  uint16_t raw = analogRead(PEDAL_PIN);
  uint8_t mapped = (uint8_t)map(raw, 0, 4095, 0, 255);
  return (mapped < 50) ? 0 : mapped;
}

// ── WebSocket event handler ────────────────────────────────────
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
    isBlocked = false;
    stopMotor();
    steerStraight();
  } else if (type == WS_EVT_DATA) {
    lastWsMsgMs = millis();
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT)
      return;

    char buf[128];
    size_t copyLen = min(len, sizeof(buf) - 1);
    memcpy(buf, data, copyLen);
    buf[copyLen] = '\0';

    // Protocol: one or more commands separated by ";". Each command:
    // "left" | "right" | "center" | "forward <0-255>" | "backward <0-255>" | "stop" | "ping"
    char *outerSave;
    char *msg = strtok_r(buf, ";", &outerSave);
    while (msg) {
      char *innerSave;
      char *cmd = strtok_r(msg, " ", &innerSave);
      if (cmd) {
        if (strcmp(cmd, "ping") == 0) {
          // keepalive — timestamp already reset above
        } else if (strcmp(cmd, "left") == 0) {
          steerLeft(200);
        } else if (strcmp(cmd, "right") == 0) {
          steerRight(200);
        } else if (strcmp(cmd, "center") == 0) {
          steerStraight();
        } else if (strcmp(cmd, "forward") == 0) {
          char *param = strtok_r(nullptr, " ", &innerSave);
          uint8_t speed = param ? (uint8_t)constrain(atoi(param), 0, 255) : 0;
          isBlocked = true;
          setMotorForward(speed);
        } else if (strcmp(cmd, "backward") == 0) {
          char *param = strtok_r(nullptr, " ", &innerSave);
          uint8_t speed = param ? (uint8_t)constrain(atoi(param), 0, 255) : 0;
          isBlocked = true;
          setMotorBackward(speed);
        } else if (strcmp(cmd, "stop") == 0) {
          isBlocked = false;
          stopMotor();
        } else if (strcmp(cmd, "block") == 0) {
          char *param = strtok_r(nullptr, " ", &innerSave);
          isBlocked = param ? (atoi(param) != 0) : false;
          if (isBlocked) stopMotor();
        }
      }
      msg = strtok_r(nullptr, ";", &outerSave);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Car booting...");

  // Attach PWM channels, then ensure safe/off state on boot
  ledcSetup(PWM_CH_FWD,   PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_FWD_PIN,   PWM_CH_FWD);
  ledcSetup(PWM_CH_BWD,   PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_BWD_PIN,   PWM_CH_BWD);
  ledcSetup(PWM_CH_LEFT,  PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(STEER_LEFT_PIN,  PWM_CH_LEFT);
  ledcSetup(PWM_CH_RIGHT, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(STEER_RIGHT_PIN, PWM_CH_RIGHT);
  // Ensure all outputs start at zero immediately (bypass ramp on boot)
  ledcWrite(PWM_CH_FWD, 0);   ledcWrite(PWM_CH_BWD, 0);
  ledcWrite(PWM_CH_LEFT, 0);  ledcWrite(PWM_CH_RIGHT, 0);

  // ON/OFF pedal pins — pulled high; pedal ties to GND when pressed
  pinMode(ONOFF_PEDAL_FW, INPUT_PULLUP);
  pinMode(ONOFF_PEDAL_BW, INPUT_PULLUP);

  WiFiManager wm;
  // Uncomment to wipe saved credentials during development:
  // wm.resetSettings();

  bool connected = wm.autoConnect("ESP32-Car");
  if (!connected) {
    Serial.println("[WiFi] Failed to connect — running as captive portal AP");
  } else {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
  }

  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed");
  } else {
    Serial.println("[FS] LittleFS mounted");
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
  Serial.println("[HTTP] Web server started");
}

void loop() {
  if (millis() - lastWsMsgMs > WS_WATCHDOG_MS) {
    isBlocked = false;
    stopMotor();
    steerStraight();
  }

  // ON/OFF pedal with 5 ms debounce: 0 = not pressed, 1 = forward, 2 = backward
  {
    static uint8_t       rawPedal       = 0;
    static uint8_t       debouncedPedal = 0;
    static uint8_t       lastActedPedal = 0xFF;  // force action on first loop
    static unsigned long lastChangeMs   = 0;

    uint8_t curPedal = (digitalRead(ONOFF_PEDAL_FW) == LOW) ? 1 :
                       (digitalRead(ONOFF_PEDAL_BW) == LOW) ? 2 : 0;

    if (curPedal != rawPedal) {
      rawPedal     = curPedal;
      lastChangeMs = millis();
    }

    // 0 (released) is immediate; 1/2 (pressed) require 5 ms stable
    if (curPedal == 0) {
      debouncedPedal = 0;
    } else if (millis() - lastChangeMs >= 5) {
      debouncedPedal = rawPedal;
    }

    if (!isBlocked && debouncedPedal != lastActedPedal) {
      lastActedPedal = debouncedPedal;
      if (debouncedPedal == 1) {
        setMotorForward(255);
      } else if (debouncedPedal == 2) {
        setMotorBackward(255);
      } else {
        stopMotor();
      }
    }
  }

  // Ramp all PWM channels toward their targets
  currentFwd   = rampChannel(currentFwd,   targetFwd,   RAMP_STEP_FWD,   PWM_CH_FWD);
  currentBwd   = rampChannel(currentBwd,   targetBwd,   RAMP_STEP_BWD,   PWM_CH_BWD);
  currentLeft  = rampChannel(currentLeft,  targetLeft,  RAMP_STEP_STEER, PWM_CH_LEFT);
  currentRight = rampChannel(currentRight, targetRight, RAMP_STEP_STEER, PWM_CH_RIGHT);

  static uint8_t lastSpeed = 255;  // force first send
  uint8_t pedalSpeed = readPedal();
  if (pedalSpeed != lastSpeed) {
    lastSpeed = pedalSpeed;
    ws.textAll("speed " + String(pedalSpeed));
  }

  static unsigned long lastRssiMs = 0;
  if (millis() - lastRssiMs >= 1000) {
    lastRssiMs = millis();
    uint8_t risCurrent = (uint8_t)(analogRead(MOTOR_RIS_PIN) >> 4);
    uint8_t lisCurrent = (uint8_t)(analogRead(MOTOR_LIS_PIN) >> 4);
    ws.textAll("rssi " + String(WiFi.RSSI()) +
               ";current_fwd " + String(risCurrent) +
               ";current_bwd " + String(lisCurrent));
  }
  ws.cleanupClients();
  delay(5);
}
