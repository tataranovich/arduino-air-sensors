#include "config.h"

#ifdef HAVE_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include <SPI.h>
#include <Wire.h>

#ifdef HAVE_SSD1306
#define HAVE_DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h>

Adafruit_SSD1306 display(128, 64);
#endif

#ifdef HAVE_MH_Z19
#define HAVE_CO2_SENSOR
#define CO2_MIN 100
#define CO2_MAX 6000

#if defined(HAVE_MH_Z19) && defined(SWAP_UART0)
  #define DPRINT(...)
  #define DPRINTLN(...)
#else
  #define DPRINT(...)    Serial.print(__VA_ARGS__)
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)
#endif

int sensorCO2() {
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9];

  Serial.write(cmd, 9); //request PPM CO2
  Serial.readBytes(response, 9);

  if (response[0] != 0xFF)
  {
    DPRINTLN(F("Incorrect start byte from MH-Z19 sensor"));
    return -1;
  }

  if (response[1] != 0x86)
  {
    DPRINTLN(F("Incorrect response from MH-Z19 sensor"));
    return -1;
  }

  int responseHigh = (int) response[2];
  int responseLow = (int) response[3];
  int ppm = (256 * responseHigh) + responseLow;
  return ppm;
}
#endif

#ifdef HAVE_DHT22
#include <DHT.h>

#define HAVE_TEMP_SENSOR
#define HAVE_HUMID_SENSOR
#define TEMP_MIN -40
#define TEMP_MAX 50
#define HUMID_MIN 0
#define HUMID_MAX 100

DHT dht(DHT22_PIN, DHT22);

float sensorTemperature() {
  return dht.readTemperature();
}

float sensorHumidity() {
  return dht.readHumidity();
}
#endif

#ifdef HAVE_SI7021
#include <Adafruit_Si7021.h>

#define HAVE_TEMP_SENSOR
#define HAVE_HUMID_SENSOR
#define TEMP_MIN -40
#define TEMP_MAX 50
#define HUMID_MIN 0
#define HUMID_MAX 100

Adafruit_Si7021 si7021 = Adafruit_Si7021();

float sensorTemperature() {
  return si7021.readTemperature();
}

float sensorHumidity() {
  return si7021.readHumidity();
}
#endif

#ifdef HAVE_BME280
#include <Adafruit_BME280.h>

#define HAVE_TEMP_SENSOR
#define HAVE_HUMID_SENSOR
#define TEMP_MIN -40
#define TEMP_MAX 50
#define HUMID_MIN 0
#define HUMID_MAX 100

Adafruit_BME280 bme;

float sensorTemperature() {
  return bme.readTemperature();
}

float sensorHumidity() {
  return bme.readHumidity();
}
#endif

String webString;
ESP8266WebServer server(80);

float humidity, temp_c, co2_ppm;
bool health_status;

unsigned long currentMillis, previousMillis;
const long updateInterval = 5000; // update sensors every 5s
#ifdef HAVE_SSD1306
unsigned long displayMillis;
bool displayActive;
const long displayTimeout = 15000; // show values on screen during 15s
#endif

void action_index() {
  server.send(200, "text/html", "<!DOCTYPE HTML><html><head><meta charset=\"utf-8\" /><title>Sensors server</title></head><body><h1>Sensors server</h1><br>You could get <a href=\"/temp\">temperature</a>, <a href=\"/humidity\">humidity</a> or <a href=\"/co2\">CO<sub>2</sub></a> values.</body></hml>");
  delay(100);
}

void action_temp() {
  #ifdef HAVE_TEMP_SENSOR
  update_sensors();
  if (health_status) {
    webString = "Temperature: " + String(temp_c) + " C";
  } else {
    webString = "Temperature: nan C";
  }
  server.send(200, "text/plain", webString);
  #else
  server.send(404, "text/plain", "Temperature is not supported");
  #endif
}

void action_humidity() {
  #ifdef HAVE_HUMID_SENSOR
  update_sensors();
  if (health_status) {
    webString = "Humidity: " + String(humidity) + "%";
  } else {
    webString = "Humidity: nan%";
  }
  server.send(200, "text/plain", webString);
  #else
  server.send(404, "text/plain", "Humidity is not supported");
  #endif
}

void action_co2() {
  #ifdef HAVE_CO2_SENSOR
  update_sensors();
  if (health_status) {
    webString = "CO2: " + String(co2_ppm) + " ppm";
  } else {
    webString = "CO2: nan ppm";
  }
  server.send(200, "text/plain", webString);
  #else
  server.send(404, "text/plain", "CO2 is not supported");
  #endif
}

void action_status_health() {
  if (health_status) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(500, "text/plain", "FAIL");
  }
}

void update_sensors() {
  #ifdef HAVE_HUMID_SENSOR
  float _humidity;
  #endif

  #ifdef HAVE_TEMP_SENSOR
  float _temp_c;
  #endif

  #ifdef HAVE_CO2_SENSOR
  int _co2_ppm;
  #endif

  currentMillis = millis();
  if (previousMillis > currentMillis) {
    previousMillis = 0;
    DPRINTLN(F("millis() overflow"));
  }
  if (currentMillis - previousMillis >= updateInterval) {
    DPRINT(F("Refreshing sensors at "));
    DPRINT(currentMillis);
    DPRINTLN(F(" msec"));
    previousMillis = currentMillis;
    health_status = false;

    #ifdef HAVE_TEMP_SENSOR
    _temp_c = sensorTemperature();
    if (isnan(_temp_c) || _temp_c < TEMP_MIN || _temp_c > TEMP_MAX) {
      DPRINTLN(F("Failed to read temperature"));
      return;
    }
    temp_c = _temp_c;
    #endif

    #ifdef HAVE_HUMID_SENSOR
    _humidity = sensorHumidity();
    if (isnan(_humidity) || _humidity < HUMID_MIN || _humidity > HUMID_MAX) {
      DPRINTLN(F("Failed to read humidity"));
      return;
    }
    humidity = _humidity;
    #endif

    #ifdef HAVE_CO2_SENSOR
    _co2_ppm = sensorCO2();
    if (isnan(_co2_ppm) || _co2_ppm < CO2_MIN || _co2_ppm > CO2_MAX) {
      DPRINTLN(F("Failed to read CO2"));
      return;
    }
    co2_ppm = _co2_ppm;
    #endif

    health_status = true;
    update_display();
  } else {
    DPRINTLN(F("Using cached sensor values"));
  }
}

void update_display() {
  #ifdef HAVE_SSD1306
  char str_temp[6];
  char str_humidity[6];
  char str_co2[6];
  currentMillis = millis();
  if (displayMillis > currentMillis && !displayActive) {
    display.clearDisplay();
    dtostrf(temp_c, 5, 1, str_temp);
    dtostrf(humidity, 5, 1, str_humidity);
    dtostrf(co2_ppm, 5, 0, str_co2);
    display.setCursor(0, 10);
    display.setFont(&FreeMono9pt7b);

    #ifdef HAVE_TEMP_SENSOR
    display.print("T  ");
    display.print(str_temp);
    display.println("C");
    #endif

    #ifdef HAVE_HUMID_SENSOR
    display.print("RH ");
    display.print(str_humidity);
    display.println("%");
    #endif

    #ifdef HAVE_CO2_SENSOR
    display.print("CO2");
    display.print(str_co2);
    display.println("ppm");
    #endif

    display.ssd1306_command(SSD1306_DISPLAYON);
    display.display();
    displayActive = true;
  } else {
    if (displayMillis <= currentMillis && displayActive) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      displayActive = false;
    }
  }
  #endif
}

void setup() {
  #ifdef HAVE_SSD1306
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setFont();
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  #endif

  Serial.begin(9600);
  #ifdef SWAP_UART0
  // See https://arduino-esp8266.readthedocs.io/en/latest/reference.html#serial for details
  // Switch UART0 from GPIO1 (TX) and GPIO3 (RX) to GPIO15 (TX) and GPIO13 (RX)
  Serial.swap();
  #endif

  #if defined(HAVE_BME280) || defined(HAVE_SI7021)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  #endif

  #ifdef HAVE_DHT22
  dht.begin();
  #endif

  #ifdef HAVE_SI7021
  si7021.begin();
  #endif

  #ifdef HAVE_BME280
  bme.begin(BME280_I2C_ADDRESS);
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  DPRINT("\n\r\n\rWorking to connect");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DPRINT(".");
  }

  DPRINTLN("");
  DPRINTLN("Air sensors on esp8266");
  DPRINT("Connected to ");
  DPRINTLN(wifi_ssid);

  #ifdef HAVE_SSD1306
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.println(wifi_ssid);
  display.display();
  #endif

  server.on("/", action_index);
  server.on("/temp", action_temp);
  server.on("/humidity", action_humidity);
  server.on("/co2", action_co2);
  server.on("/status/health", action_status_health);

  server.begin();
  DPRINT("Server started at http://");
  DPRINTLN(WiFi.localIP());

  #ifdef HAVE_SSD1306
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  // initial delay for device stabilization
  display.println("Sensors preheat...");
  display.display();
  #endif
  delay(updateInterval);

  #ifdef HAVE_SSD1306
  // OLED burnout protection
  pinMode(0, INPUT);
  displayMillis = millis() + displayTimeout;
  displayActive = false;
  #endif

  update_sensors();
}

void loop() {
  server.handleClient();
  #ifdef HAVE_SSD1306
  if (digitalRead(0) == LOW) {
    displayMillis = millis() + displayTimeout;
    update_sensors();
  }
  update_display();
  #endif
}
