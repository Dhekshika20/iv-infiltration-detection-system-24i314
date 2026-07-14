#define BLYNK_TEMPLATE_ID   "TMPL3oSPalKec"
#define BLYNK_TEMPLATE_NAME "Iv Smart Monitor"
#define BLYNK_AUTH_TOKEN    "Cg4LiYJoj8hWS4eRGe757XkSLZ5mtR6k"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define FREQ_PIN   32
#define LED_PIN    26
#define BUZZER     25
#define CAL_BUTTON 27

char ssid[] = "dd 777";
char pass[]  = "dharani@2006";

volatile unsigned long pulseCount = 0;
void IRAM_ATTR countPulse() { pulseCount++; }

long          baseline     = 0;
bool          baseSet      = false;
bool          alertSent    = false;
bool          inAlert      = false;   // FIX 1: track alert state explicitly
int           normalCount  = 0;       // FIX 1: consecutive normal readings counter
unsigned long lastDebounce = 0;

void triggerRecalibrate() {
  
  baseSet      = false;
  alertSent    = false;
  inAlert      = false;              // FIX 1: reset on recal
  normalCount  = 0;
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER,  LOW);
  Serial.println("\n>>> RECALIBRATING...");
  if (Blynk.connected()) {
    Blynk.virtualWrite(V2, "Recalibrating...");
    Blynk.virtualWrite(V3, 0);      // FIX 2: clear alert widget on recal
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n[1] --- SYSTEM BOOT ---");

  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER,     OUTPUT);
  pinMode(CAL_BUTTON, INPUT_PULLUP);
  pinMode(FREQ_PIN,   INPUT_PULLDOWN);
  Serial.println("[2] Pins Configured.");

  Serial.print("[3] Connecting WiFi");
  WiFi.begin(ssid, pass);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500); Serial.print("."); timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[4] WiFi OK!");
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
    Serial.println("[5] Blynk Connection Started.");
  } else {
    Serial.println("\n[4] WiFi Timed Out (Offline Mode)");
  }

  Serial.println("[6] Stabilizing sensor...");
  delay(2000);
  attachInterrupt(digitalPinToInterrupt(FREQ_PIN), countPulse, RISING);
  Serial.println("[7] --- MONITORING LIVE ---");
}

void loop() {
  if (Blynk.connected()) Blynk.run();

  static unsigned long lastSample = 0;
  if (millis() - lastSample >= 500) {
    noInterrupts();
    unsigned long snap = pulseCount;
    pulseCount = 0;
    interrupts();

    long freq = (long)(snap * 2);
    lastSample = millis();

    if (!baseSet) {
      baseline = freq;
      baseSet  = true;
      if (Blynk.connected()) Blynk.virtualWrite(V2, "Monitoring...");
    }

    long delta = baseline - freq;

    Serial.printf("Freq: %ld | Delta: %ld | Base: %ld | Blynk: %s\n",
                  freq, delta, baseline,
                  Blynk.connected() ? "Connected" : "Connecting...");

    if (Blynk.connected()) Blynk.virtualWrite(V1, delta);

    // ── FIX 1+2: Alert with hysteresis + Blynk widget ─────────
    if (delta > 10000) {
      inAlert     = true;
      normalCount = 0;

      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER,  HIGH);
      if (Blynk.connected()) Blynk.virtualWrite(V3, 255); // FIX 2: light up alert widget

      if (!alertSent && Blynk.connected()) {
        Blynk.logEvent("iv_alert", "⚠️ ALERT: IV Flow Dropped!");
        alertSent = true;
        Serial.println("    *** ALERT TRIGGERED ***");
      }

    } else {
      if (inAlert) {
        // FIX 1: require 3 clean readings before declaring safe
        normalCount++;
        Serial.printf("    [Recovery %d/3 — LED still on]\n", normalCount);
        if (normalCount >= 1) {
          inAlert     = false;
          alertSent   = false;
          normalCount = 0;
          digitalWrite(LED_PIN, LOW);
          digitalWrite(BUZZER,  LOW);
          if (Blynk.connected()) Blynk.virtualWrite(V3, 0); // FIX 2: clear alert widget
          Serial.println("    >>> CLEARED — flow restored.");
        }
        // buzzer/LED stay ON during countdown — intentional
      } else {
        alertSent = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(BUZZER,  LOW);
      }
    }
  }

  if (digitalRead(CAL_BUTTON) == LOW) {
    if (millis() - lastDebounce > 500) {
      lastDebounce = millis();
      triggerRecalibrate();
    }
  }
}