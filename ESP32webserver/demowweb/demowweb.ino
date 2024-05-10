#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "DACN1";
const char* password = "11111111";

int setpoint = 0;
int hysteresis = 0;

const int HEATING_PIN = 18;
const int COOLING_PIN = 19;

#define ONE_WIRE_BUS 23
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

bool automaticMode = false;
bool heaterbtn = false;
bool coolerbtn = false;

AsyncWebServer server(80);
ESPDash dashboard(&server);

Card temperature(&dashboard, TEMPERATURE_CARD, "Nhiệt độ hiện tại", "°C");
Card tempSet(&dashboard, TEMPERATURE_CARD, "Điểm đặt nhiệt độ", "°C");
Card tempHyss(&dashboard, TEMPERATURE_CARD, "Điểm đặt độ trễ", "°C");
Card setTemp(&dashboard, SLIDER_CARD, "Nhiệt độ", "", 0, 50);
Card setHyss(&dashboard, SLIDER_CARD, "Độ trễ", "", 0, 5);
Card autoMode(&dashboard, BUTTON_CARD, "Auto Mode");
Card ModeStatus(&dashboard, STATUS_CARD, "Auto Mode Status");
Card Heater(&dashboard, BUTTON_CARD, "Máy sưởi");
Card Cooler(&dashboard, BUTTON_CARD, "Máy lạnh");

unsigned long heaterStartTime = 0;
unsigned long heaterDuration = 0;
unsigned long coolerStartTime = 0;
unsigned long coolerDuration = 0;

void setup() {
  Serial.begin(115200);
  pinMode(HEATING_PIN, OUTPUT);
  pinMode(COOLING_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.println("IP: ");
  display.setCursor(22, 2);
  display.println(WiFi.softAPIP());
  display.display();

  digitalWrite(HEATING_PIN, HIGH);
  Serial.println("Heater OFF");
  digitalWrite(COOLING_PIN, HIGH);
  Serial.println("Cooler OFF");

  server.begin();
  sensors.begin();
}

void loop() {
  sensors.requestTemperatures(); 
  float currentTemp = sensors.getTempCByIndex(0);
  dashboard.setAuthentication("admin", "1234");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.println("IP: ");
  display.setCursor(22, 2);
  display.println(WiFi.softAPIP());
  display.display();
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 17);
  display.println(currentTemp);
  display.println(" ");
  display.drawRect(90, 17, 5, 5, WHITE);
  display.setCursor(97, 17);
  display.println("C");
  display.display();

  temperature.update((int)(currentTemp));
  tempSet.update((int) (setpoint));
  tempHyss.update((int) (hysteresis));

  setTemp.attachCallback([&](int value) {
    Serial.println("[setTemp] Slider Callback Triggered: " + String(value));
    setpoint = value;
    setTemp.update(value);
    dashboard.sendUpdates();
  });

  setHyss.attachCallback([&](int value) {
    Serial.println("[setHyss] Slider Callback Triggered: " + String(value));
    hysteresis = value;
    setHyss.update(value);
    dashboard.sendUpdates();
  });

  autoMode.attachCallback([&](int value) {
    Serial.println("[autoMode] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    automaticMode = value;
    autoMode.update(value);
    dashboard.sendUpdates();
  });

  Heater.attachCallback([&](int value) {
    Serial.println("[Heater] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    heaterbtn = value;
    if (automaticMode == 0 && heaterbtn == 1) {
      heaterStartTime = millis();
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Heater ON");
    } else if (automaticMode == 0 && heaterbtn == 0) {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
    }
    Heater.update(value);
    dashboard.sendUpdates();
  });

  Cooler.attachCallback([&](int value) {
    Serial.println("[Cooler] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    coolerbtn = value;
    if (automaticMode == 0 && coolerbtn == 1) {
      coolerStartTime = millis();
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Cooler ON");
    } else if (automaticMode == 0 && coolerbtn == 0) {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    }
    Cooler.update(value);
    dashboard.sendUpdates();
  });

  if (automaticMode == 1) {
    // Automatic Mode Logic Here
  } else {
    // Manual Mode Logic Here
    if (heaterbtn == 1 && millis() - heaterStartTime >= heaterDuration) {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
    }
    if (coolerbtn == 1 && millis() - coolerStartTime >= coolerDuration) {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    }
  }

  dashboard.sendUpdates();
  delay(3000);
}
