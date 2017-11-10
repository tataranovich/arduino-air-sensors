#include "wifi-config.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DHT22_PIN 14
#define MHZ19_TX_PIN 12
#define MHZ19_RX_PIN 13

String webString;
ESP8266WebServer server(80);
DHT dht(DHT22_PIN, DHT22);
SoftwareSerial co2Serial(MHZ19_TX_PIN, MHZ19_RX_PIN);
float humidity, temp_c, co2_ppm;
bool health_status;
Adafruit_SSD1306 display;

unsigned long currentMillis, previousMillis;
// update sensors every 5s
const long updateInterval = 5000;

void action_index() {
  server.send(200, "text/html", "<!DOCTYPE HTML><html><head><meta charset=\"utf-8\" /><title>Sensors server</title></head><body><h1>Sensors server</h1><br>You could get <a href=\"/temp\">temperature</a>, <a href=\"/humidity\">humidity</a> or <a href=\"/co2\">CO<sub>2</sub></a> values.</body></hml>");
  delay(100);
}

void action_temp() {
  update_sensors();
  if (health_status) {
    webString = "Temperature: " + String(temp_c) + " C";
  } else {
    webString = "Temperature: nan C";
  }
  server.send(200, "text/plain", webString);
}

void action_humidity() {
  update_sensors();
  if (health_status) {
    webString = "Humidity: " + String(humidity) + "%";
  } else {
    webString = "Humidity: nan%";
  }
  server.send(200, "text/plain", webString);
}

void action_co2() {
  update_sensors();
  if (health_status) {
    webString = "CO2: " + String(co2_ppm) + " ppm";
  } else {
    webString = "CO2: nan ppm";
  }
  server.send(200, "text/plain", webString);
}

void action_status_health() {
  if (health_status) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(500, "text/plain", "FAIL");
  }
}

void update_sensors() {
  float _humidity;
  float _temp_c;
  int _co2_ppm;

  currentMillis = millis();
  if (previousMillis > currentMillis) {
    previousMillis = 0;
    Serial.println(F("millis() overflow"));
  }
  if (currentMillis - previousMillis >= updateInterval) {
    Serial.print(F("Refreshing sensors at "));
    Serial.print(currentMillis);
    Serial.println(F(" msec"));
    previousMillis = currentMillis;
    health_status = false;

    _humidity = dht.readHumidity();
    _temp_c = dht.readTemperature();

    if (isnan(_humidity) || isnan(_temp_c)) {
      Serial.println(F("Failed to read data from DHT22"));
      return;
    }

    // DHT22 measurements range
    // Humidity: 0..100%
    // Temperature: -40C..50C
    if (_temp_c < -40.0 || _temp_c >= 50.0) {
      Serial.println(F("Temperature value readed from DHT22 is out of range -40C .. 50C"));
      return;
    }
    temp_c = _temp_c;

    if (_humidity < 0.0 || _humidity > 100) {
      Serial.println(F("Humidity value readed from DHT22 is out of range 0%..100%"));
      return;
    }
    humidity = _humidity;

    _co2_ppm = readCO2();
    if (isnan(_co2_ppm)) {
      Serial.println(F("Failed to read data from MH-Z19"));
      return;
    }
    if (_co2_ppm < 100 || _co2_ppm > 6000) {
      Serial.println(F("CO2 value readed from MH-Z19 is out of range 100..5000 ppm"));
      return;
    }
    co2_ppm = _co2_ppm;
    health_status = true;
    refresh_display();
  } else {
    Serial.println(F("Using cached sensor values"));
  }
}

int readCO2()
{
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9];

  co2Serial.write(cmd, 9); //request PPM CO2
  co2Serial.readBytes(response, 9);

  for (int i = 0; i < 9; i++) {
    Serial.print(F("0x"));
    Serial.print(response[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println("");

  if (response[0] != 0xFF)
  {
    Serial.println(F("Incorrect start byte from MH-Z19 sensor"));
    return -1;
  }

  if (response[1] != 0x86)
  {
    Serial.println(F("Incorrect response from MH-Z19 sensor"));
    return -1;
  }

  int responseHigh = (int) response[2];
  int responseLow = (int) response[3];
  int ppm = (256 * responseHigh) + responseLow;
  return ppm;
}

void refresh_display() {
  char str_temp[6];
  char str_humidity[6];
  char str_co2[6];
  dtostrf(temp_c, 4, 1, str_temp);
  dtostrf(humidity, 4, 1, str_humidity);
  dtostrf(co2_ppm, 4, 0, str_co2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(str_temp);
  display.println("C");
  display.print("  RH: ");
  display.print(str_humidity);
  display.println("%");
  display.print(" CO2: ");
  display.print(str_co2);
  display.println("ppm");
  display.display();
}

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setFont();
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  Serial.begin(9600);
  co2Serial.begin(9600);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("\n\r\n\rWorking to connect");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Air sensors on esp8266");
  Serial.print("Connected to ");
  Serial.println(ssid);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.println(ssid);
  display.display();

  server.on("/", action_index);
  server.on("/temp", action_temp);
  server.on("/humidity", action_humidity);
  server.on("/co2", action_co2);
  server.on("/status/health", action_status_health);

  server.begin();
  Serial.print("Server started at http://");
  Serial.println(WiFi.localIP());
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();

  // initial delay for device stabilization
  display.println("Sensors preheat...");
  display.display();
  delay(updateInterval);
  update_sensors();
}

void loop() {
  server.handleClient();
}
