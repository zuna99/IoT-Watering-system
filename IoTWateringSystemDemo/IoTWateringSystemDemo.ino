/*************************************************************
   ESP32 Blynk IoT Pump Control
*************************************************************/

#define BLYNK_TEMPLATE_ID   "TMPL4NxcLZUta"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN    "***" //Add auth token

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "esp_sleep.h"

// WiFi credentials
char ssid[] = "***";  //Add SSID
char pass[] = "***"; //Add password

#define PUMP_PIN 12
#define MOISTURE_PIN 36   // Soil sensor analog input (A0 = GPIO36)
#define TANK_PIN     26   // Tank sensor digital input

//Variables
int moisture = 0;
bool pumpIsOn = false;
unsigned long pumpStartTime = 0;
const unsigned long maxPumpTime = 3000; // 3 seconds
bool justWokeUp = true; // true on wake
unsigned long lastPumpActionTime = 0;  // time pump last turned ON or OFF
const unsigned long idleBeforeSleep = 10000; // 10 sec

BlynkTimer timer;

BLYNK_WRITE(V0) {
  int value = param.asInt();       // Value from Blynk button
  //digitalWrite(PUMP_PIN, value);   // Control pump pin
  //Serial.println(value ? "Pump ON" : "Pump OFF");

  if (value == 1 && !pumpIsOn) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpIsOn = true;
    pumpStartTime = millis();
    lastPumpActionTime = millis();
    Serial.println("Pump turned ON by app");
  } else if (value == 0 && pumpIsOn) {
    digitalWrite(PUMP_PIN, LOW);
    pumpIsOn = false;
    lastPumpActionTime = millis();
    Serial.println("Pump turned OFF by app");
  }
}

// --- Sync V0 after connection ---
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0); // ask for last pump state
}

void checkPumpTimeout() {
  if (pumpIsOn && millis() - pumpStartTime >= maxPumpTime) {
    digitalWrite(PUMP_PIN, LOW);
    pumpIsOn = false;
    Blynk.virtualWrite(V0, 0); // turn off button in app
    Serial.println("Pump auto-stopped after 10s");
  }
}

void readMoisture() {
  int sensorValue = analogRead(MOISTURE_PIN);
  moisture = 100 - ((sensorValue / 4095.0) * 100);
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.println(" %");

  Blynk.virtualWrite(V1, moisture); // Send to app (V1)
}

void readTankStatus() {
  // In your timed task (e.g., every 2 seconds)
  bool tankEmpty = (digitalRead(TANK_PIN) == LOW);
  //Blynk.virtualWrite(V2, tankEmpty ? "Empty" : "OK");
  Serial.print("Tank: ");
  Serial.println(tankEmpty ? "EMPTY" : "OK");

  if (tankEmpty) {
    Blynk.virtualWrite(V2, "EMPTY"); // label display
    Blynk.virtualWrite(V3, 255);     // LED ON
    Blynk.setProperty(V3, "color", "#FF0000"); // Red
  } else {
    Blynk.virtualWrite(V2, "OK");
    Blynk.virtualWrite(V3, 255);     // LED ON
    Blynk.setProperty(V3, "color", "#00FF00"); // Green
  }
}

void goToSleep() {
  Serial.println("Going to sleep for 2 minutes...");
  delay(100);
  esp_sleep_enable_timer_wakeup(120ULL * 1000000ULL); // 2 min in microseconds
  esp_deep_sleep_start();
}

// Call this regularly in timer
void checkDeepSleepTimeout() {
  if (!pumpIsOn) {
    unsigned long idleTime = millis() - lastPumpActionTime;
    if (idleTime >= idleBeforeSleep) {
      Serial.println("No pump activity for 10s, going to sleep");
      goToSleep();
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(TANK_PIN, INPUT_PULLUP);
  
  digitalWrite(PUMP_PIN, LOW);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Run once after wake: read & send sensor data
  readMoisture();
  readTankStatus();

  // If pump is OFF after 10 sec → sleep again
  timer.setTimeout(10000L, []() {
    if (!pumpIsOn) {
      goToSleep();
    }
  });

  // Read sensors every 2 seconds
  timer.setInterval(2000L, readMoisture);
  timer.setInterval(2000L, readTankStatus);
  timer.setInterval(500L, checkPumpTimeout);
  lastPumpActionTime = millis(); // start idle timer
  timer.setInterval(1000L, checkDeepSleepTimeout); // check every 1s
}

void loop() {
  Blynk.run();
  timer.run();
  //delay(2000);
}
