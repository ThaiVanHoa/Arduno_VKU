#include <Arduino.h>


/* ESP32 Dependencies */
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
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* Your SoftAP WiFi Credentials */
const char* ssid = "DACN1"; // SSID
const char* password = "11111111"; // Password

// Setpoint and hysteresis values (in degrees Celsius)
int setpoint = 0;
int hysteresis = 0;

// Pin numbers for the heating and cooling relays
const int HEATING_PIN = 18; //relay1
const int COOLING_PIN = 19;  //relay2

#define ONE_WIRE_BUS 23 //BD128 Sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Control mode (automatic or manual)
bool automaticMode = 0;
bool heaterbtn = 0;
bool coolerbtn = 0;

/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(&server);
/*
  Dashboard Cards
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card temperature(&dashboard, TEMPERATURE_CARD, "Nhiệt độ hiện tại", "°C");
Card tempSet(&dashboard, TEMPERATURE_CARD, "Điểm đặt nhiệt độ", "°C");
Card tempHyss(&dashboard, TEMPERATURE_CARD, "Điểm đặt độ trễ", "°C");
Card setTemp(&dashboard, SLIDER_CARD, "Nhiệt độ", "", 0, 50);
Card setHyss(&dashboard, SLIDER_CARD, "Độ trễ", "", 0, 5);
Card autoMode(&dashboard, BUTTON_CARD, "Auto Mode");
Card ModeStatus(&dashboard, STATUS_CARD, "Auto Mode Status");
Card Heater(&dashboard, BUTTON_CARD, "Máy sưởi");
Card Cooler(&dashboard, BUTTON_CARD, "Máy lạnh");
void setup() {
  Serial.begin(115200);

  // Initialize the relays and temperature sensor
  pinMode(HEATING_PIN, OUTPUT);
  pinMode(COOLING_PIN, OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();

  /* Start Access Point */
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  display.setTextSize(1); // Draw 2X-scale text
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

  /* Start AsyncWebServer */
  server.begin();

    // Start up the library
  sensors.begin();
}

void loop() {
  // Send the command to get temperatures
  sensors.requestTemperatures(); 
  float currentTemp = sensors.getTempCByIndex(0);
  
  // Set Authentication for Dashboard
  dashboard.setAuthentication("admin", "1234");

  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.println("IP: ");
  display.setCursor(22, 2);
  display.println(WiFi.softAPIP());
  display.display();
  display.setTextSize(3); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 17);
  display.println(currentTemp);
  display.println(" ");
  display.drawRect(90, 17, 5, 5, WHITE);     // put degree symbol ( ° )
  display.setCursor(97, 17);
  display.println("C");
  display.display();

  /* Update Card Values */
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

  Serial.println(hysteresis);
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("Hys: ");
  display.print(hysteresis);
  display.display();
  Serial.println(setpoint);
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(70, 55);
  display.print("Temp: ");
  display.print(setpoint);
  display.display();

  autoMode.attachCallback([&](int value) {
    Serial.println("[autoMode] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    automaticMode = value;
    autoMode.update(value);
    dashboard.sendUpdates();
  });

  if (automaticMode == 1) {
    Serial.println("Auto Mode Enabled");
    ModeStatus.update("Enabled", "success");
    dashboard.sendUpdates();
  }
  if (automaticMode == 0) {
    Serial.println("Auto Mode Disabled");
    ModeStatus.update("Disabled", "danger");
    dashboard.sendUpdates();
  }
  Heater.attachCallback([&](int value) {
    Serial.println("[Heater] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    heaterbtn = value;
    if (automaticMode == 0 && heaterbtn == 1)
    {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Heater ON");
    }
    else if (automaticMode == 0 && heaterbtn == 0)
    {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
    }
    Heater.update(value);
    dashboard.sendUpdates();
  });

  Cooler.attachCallback([&](int value) {
    Serial.println("[Cooler] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    coolerbtn = value;
    if (automaticMode == 0 && coolerbtn == 1)
    {
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Cooler ON");
    }
    else if (automaticMode == 0 && coolerbtn == 0)
    {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    }
    Cooler.update(value);
    dashboard.sendUpdates();
  });

  // If in automatic mode, control the heating and cooling relays based on the setpoint and hysteresis
  if (automaticMode == 1) {
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(28, 45);
    display.print("Auto Mode");
    display.display();

    if (currentTemp > setpoint + hysteresis) {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Cooler ON");
    } else if (currentTemp < setpoint - hysteresis) {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Heater ON");
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    } else {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    }
  }
  else if (automaticMode == 0)
  {
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(25, 45);
    display.print("Manual Mode");
    display.display();
    if (heaterbtn == 1) {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Heater ON");
    }
    else {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Heater OFF");
    }
    if (coolerbtn == 1) {
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Cooler ON");
    }
    else {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Cooler OFF");
    }
  }

  /* Send Updates to our Dashboard (realtime) */
  dashboard.sendUpdates();

  /*
    Delay is just for demonstration purposes in this example,
    Replace this code with 'millis interval' in your final project.
  */
  delay(3000);
}