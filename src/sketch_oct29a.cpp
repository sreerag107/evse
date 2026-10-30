#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>


// =====================================================
// CP PWM
// =====================================================
#define CP_PWM_PIN 25

// Separate indicator LED on GPIO 13
#define LED_BLINK 13
bool LED_State = false;
unsigned long previousledblink = 0;
const unsigned long ledblinkinterval = 1000;

// =====================================================
// CP ADC
// =====================================================
#define CP_ADC_PIN 34

const float R1 = 150000.0;
const float R2 = 20000.0;
const float vdividfactor = (R1 + R2) / R2;

// =====================================================
// WIFI - SET YOUR LOCAL NETWORK CREDENTIALS
// =====================================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// =====================================================
// WS2812B
// =====================================================
#define LED_PIN 12
#define NUM_LEDS 5

Adafruit_NeoPixel strip(
  NUM_LEDS,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);

// =====================================================
// RFID RC522
// =====================================================
#define RFID_SS_PIN 5
#define RFID_RST_PIN 22

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

byte authorizedUID[] = {
  0x39, 0xB3, 0xFD, 0x06
};

const byte authorizedUIDLength = sizeof(authorizedUID);

// =====================================================
// EMERGENCY SWITCH AND RELAY
// =====================================================
#define EMERGENCY_PIN 14
#define RELAY 26

// Emergency input uses INPUT_PULLUP:
// pressed = LOW, released = HIGH.

// =====================================================
// CP STATES - YOUR MEASURED ADC THRESHOLDS
// =====================================================
enum CPState {
  CP_State_A,
  CP_State_B,
  CP_State_C,
  CP_State_D,
  CP_State_E,
  CP_State_U
};

CPState cpState = CP_State_U;

// =====================================================
// ACCESS AND EMERGENCY STATUS
// =====================================================
bool authorizedAccess = false;
bool emergencyActive = false;

// =====================================================
// UNAUTHORIZED CARD: GREEN BLINK FOR 3 SECONDS
// =====================================================
bool unauthorizedBlinkActive = false;
bool unauthorizedLedState = false;

unsigned long unauthorizedBlinkStart = 0;
unsigned long lastUnauthorizedBlink = 0;

const unsigned long unauthorizedBlinkTime = 3000;
const unsigned long unauthorizedBlinkInterval = 150;

// =====================================================
// GREEN CHASER
// =====================================================
unsigned long lastChaserTime = 0;
int chaserPosition = 0;

const unsigned long chaserSpeed = 120;


// =====================================================
// GREEN COMET
// =====================================================
unsigned long lastCometTime = 0;
int cometPosition = 0;
const unsigned long cometSpeed = 180;
// =====================================================
// RED BLINK FOR WIFI DISCONNECTED / CP ERROR
// =====================================================
unsigned long lastRedBlink = 0;
bool redLedState = false;

const unsigned long redBlinkInterval = 500;

// =====================================================
// WIFI RECONNECT
// =====================================================
unsigned long lastWiFiReconnect = 0;
const unsigned long wifiReconnectInterval = 5000;

// =====================================================
// SERIAL PRINT INTERVAL
// =====================================================
unsigned long lastCPPrint = 0;
const unsigned long cpPrintInterval = 1000;

// =====================================================
// LED FUNCTIONS
// =====================================================
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void clearLEDs() {
  setAllLEDs(0, 0, 0);
}

void greenChaser() {
  if (millis() - lastChaserTime >= chaserSpeed) {
    lastChaserTime = millis();

    clearLEDs();

    strip.setPixelColor(
      chaserPosition,
      strip.Color(0, 255, 0)
    );
    strip.show();

    chaserPosition++;

    if (chaserPosition >= NUM_LEDS) {
      chaserPosition = 0;
    }
  }
}


void greenComet()
{
  if (millis() - lastCometTime >= cometSpeed)
  {
    lastCometTime = millis();

    // Clear all LEDs before drawing the comet
    clearLEDs();

    // Brightness levels behind the comet head
    const uint8_t brightness[] = {
      0,    // Tail end: 0%
      10,   // Tail: 10%
      25,   // Tail: 25%
      70,   // Tail: 70%
      100  // Comet head: 100%
    };

    // Draw the comet and its fading tail
    for (int tail = 0; tail < NUM_LEDS; tail++)
    {
      int ledIndex = cometPosition - tail;

      // Wrap around the 5-LED strip
      while (ledIndex < 0)
      {
        ledIndex += NUM_LEDS;
      }

      uint8_t level = brightness[tail];

      // Scale green intensity from 0-255
      uint8_t green = (255 * level) / 100;

      strip.setPixelColor(
        ledIndex,
        strip.Color(0, green, 0)
      );
    }

    strip.show();

    // Move comet to the next LED
    cometPosition++;

    if (cometPosition >= NUM_LEDS)
    {
      cometPosition = 0;
    }
  }
}



void redBlink() {
  if (millis() - lastRedBlink >= redBlinkInterval) {
    lastRedBlink = millis();
    redLedState = !redLedState;

    if (redLedState) {
      setAllLEDs(255, 0, 0);
    } else {
      clearLEDs();
    }
  }
}

void unauthorizedBlink() {
  if (millis() - lastUnauthorizedBlink >=
      unauthorizedBlinkInterval) {
    lastUnauthorizedBlink = millis();
    unauthorizedLedState = !unauthorizedLedState;

    if (unauthorizedLedState) {
      setAllLEDs(0, 255, 0);  // GREEN
    } else {
      clearLEDs();
    }
  }
}

// =====================================================
// RFID UID CHECK
// =====================================================
bool isAuthorizedCard() {
  if (rfid.uid.size != authorizedUIDLength) {
    return false;
  }

  for (byte i = 0; i < authorizedUIDLength; i++) {
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      return false;
    }
  }

  return true;
}

void printCardUID() {
  Serial.print("Card UID: ");

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }

  Serial.println();
}

// =====================================================
// CP ADC STATE DETECTION
// Thresholds preserved from your code.
// =====================================================
CPState getState(int adc) {
  if (adc >= 4000 && adc <= 4095) {
    return CP_State_A;
  } else if (adc >= 3500 && adc <= 3999) {
    return CP_State_B;
  } else if (adc >= 2500 && adc <= 3499) {
    return CP_State_C;
  } else if (adc >= 1500 && adc <= 2499) {
    return CP_State_D;
  } else if (adc <= 1499) {
    return CP_State_E;
  } else {
    return CP_State_U;
  }
}

void printCPState(CPState state) {
  switch (state) {
    case CP_State_A:
      Serial.println("STATE A - AVAILABLE");
      break;

    case CP_State_B:
      Serial.println("STATE B - PREPARING");
      break;

    case CP_State_C:
      Serial.println("STATE C - CHARGING");
      break;

    case CP_State_D:
      Serial.println("STATE D - VENTILATION REQUIRED");
      break;

    case CP_State_E:
      Serial.println("STATE E - ERROR");
      break;

    default:
      Serial.println("STATE UNKNOWN");
      break;
  }
}

// =====================================================
// CENTRAL LED PRIORITY CONTROL
//
// Priority:
// 1. Emergency: solid red
// 2. Wi-Fi disconnected: blinking red
// 3. Unauthorized card: green blink for 3 seconds
// 4. CP state and RFID authorization
// =====================================================
void updateLEDs() {
  // Highest priority: emergency
  if (emergencyActive) {
    setAllLEDs(255, 0, 0);
    return;
  }

  // Wi-Fi disconnected
  if (WiFi.status() != WL_CONNECTED) {
    redBlink();
    return;
  }

  // Temporary unauthorized-card indication
  if (unauthorizedBlinkActive) {
    if (millis() - unauthorizedBlinkStart <
        unauthorizedBlinkTime) {
      unauthorizedBlink();
      return;
    }

    unauthorizedBlinkActive = false;
    unauthorizedLedState = false;
    clearLEDs();
  }

  // Normal CP state indication
  switch (cpState) {
    case CP_State_A:
      // State A: BLUE, relay remains OFF
      setAllLEDs(0, 0, 255);
      break;

    case CP_State_B:
      // State B: ORANGE, relay remains OFF
      setAllLEDs(255, 80, 0);
      break;

    case CP_State_C:
      if (authorizedAccess) {
        // State C + authorization: GREEN CHASER
        greenChaser();
      } else {
        // State C without authorization: RED, no charging
        setAllLEDs(255, 80, 0);
      }
      break;

    case CP_State_D:
      // State D: RED constant
      setAllLEDs(255, 0, 0);
      break;

    case CP_State_E:
    case CP_State_U:
    default:
      // CP error or unknown state: blinking red
      redBlink();
      break;
  }
}

// =====================================================
// CENTRAL RELAY CONTROL
// Relay can be ON only if all conditions are satisfied.
// =====================================================
void updateRelay() {
  bool chargingPermitted =
    (WiFi.status() == WL_CONNECTED) &&
    (!emergencyActive) &&
    (cpState == CP_State_C) &&
    authorizedAccess;

  digitalWrite(RELAY, chargingPermitted ? HIGH : LOW);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_BLINK, OUTPUT);
  digitalWrite(LED_BLINK, LOW);

  pinMode(EMERGENCY_PIN, INPUT_PULLUP);

  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // CP PWM: 1 kHz, 10-bit resolution.
  // 1023 means 100% duty, not 10%.
  ledcAttach(CP_PWM_PIN, 1000, 10);
  ledcWrite(CP_PWM_PIN, 1023);

  // ADC configuration
  analogReadResolution(12);
  analogSetPinAttenuation(CP_ADC_PIN, ADC_11db);

  // WS2812B
  strip.begin();
  strip.setBrightness(30);
  clearLEDs();

  // RC522 SPI
  SPI.begin(18, 19, 23, RFID_SS_PIN);
  rfid.PCD_Init();

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.println("================================");
  Serial.println("       EVSE RFID CONTROL");
  Serial.println("================================");
  Serial.println("Connecting to Wi-Fi...");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  unsigned long now = millis();

  // ---------------------------------------------------
  // Separate indicator LED on GPIO 13
  // ---------------------------------------------------
  if (now - previousledblink >= ledblinkinterval) {
    previousledblink = now;
    LED_State = !LED_State;
    digitalWrite(LED_BLINK, LED_State ? HIGH : LOW);
  }

  // ---------------------------------------------------
  // Read emergency switch first
  // ---------------------------------------------------
  bool emergencyPressed =
    (digitalRead(EMERGENCY_PIN) == LOW);

  if (emergencyPressed) {
    if (!emergencyActive) {
      Serial.println("EMERGENCY STOP ACTIVATED");
    }

    emergencyActive = true;

    // Clear authorization immediately, regardless of CP state.
    authorizedAccess = false;

    // Cancel any temporary RFID LED animation.
    unauthorizedBlinkActive = false;

    // Force relay OFF immediately.
    digitalWrite(RELAY, LOW);
  } else {
    if (emergencyActive) {
      Serial.println("Emergency released.");
      Serial.println("RFID authorization cleared.");
      Serial.println("Scan authorized card again when ready.");
    }

    // Keep authorization cleared after emergency release.
    emergencyActive = false;
  }

  // ---------------------------------------------------
  // Wi-Fi reconnect
  // ---------------------------------------------------
  if (WiFi.status() != WL_CONNECTED) {
    // Fail safe if Wi-Fi is disconnected.
    authorizedAccess = false;
    digitalWrite(RELAY, LOW);

    if (now - lastWiFiReconnect >=
        wifiReconnectInterval) {
      lastWiFiReconnect = now;
      Serial.println("Wi-Fi disconnected. Reconnecting...");
      WiFi.reconnect();
    }
  }

  // ---------------------------------------------------
  // Read CP ADC and update CP state
  // ---------------------------------------------------
  int cpADC = analogRead(CP_ADC_PIN);

  float cpPinVoltage =
    analogReadMilliVolts(CP_ADC_PIN) / 1000.0;

  float cpInputVoltage = cpPinVoltage * vdividfactor;

  CPState newState = getState(cpADC);

  if (newState != cpState) {
    cpState = newState;
    chaserPosition = 0;
    lastChaserTime = 0;
  }

  // Print measurements every 500 ms, not every loop.
  if (now - lastCPPrint >= cpPrintInterval) {
    lastCPPrint = now;

    Serial.print("CP ADC = ");
    Serial.println(cpADC);

    Serial.print("ADC pin voltage = ");
    Serial.print(cpPinVoltage, 3);
    Serial.println(" V");

  

    printCPState(cpState);
  }

  // ---------------------------------------------------
  // RFID card detection
  // Do not accept cards while emergency is active.
  // ---------------------------------------------------
  if (!emergencyActive &&
      rfid.PICC_IsNewCardPresent() &&
      rfid.PICC_ReadCardSerial()) {

    printCardUID();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Access denied: Wi-Fi disconnected.");
    } else if (isAuthorizedCard()) {
      // Valid card toggles authorization.
      if (!authorizedAccess) {
        authorizedAccess = true;

        chaserPosition = 0;
        lastChaserTime = 0;

        Serial.println("Authorized RFID card.");
        Serial.println("Authorization enabled.");

        // Relay is controlled by updateRelay().
        // It turns ON only when CP state is C.
      } else {
        authorizedAccess = false;

        digitalWrite(RELAY, LOW);

        Serial.println("Authorized RFID card tapped again.");
        Serial.println("Charging authorization cleared.");
        Serial.println("Relay OFF.");
      }
    } else {
      // Unauthorized card: green blink for 3 seconds.
      Serial.println("Unauthorized RFID card.");

      unauthorizedBlinkActive = true;
      unauthorizedBlinkStart = millis();
      lastUnauthorizedBlink = 0;
      unauthorizedLedState = false;
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // ---------------------------------------------------
  // Apply relay rules, then update LEDs by priority
  // ---------------------------------------------------
  updateRelay();
  updateLEDs();

  delay(10);
}
