#include <MemoryFree.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,177);
EthernetServer server(80);
String request;
DHT dht(2, DHT22);
SoftwareSerial co2Serial(8, 9);
float humidity, temp_c, co2_ppm;
bool health_status;

unsigned long currentMillis, previousMillis;
// update sensors every 5s
const long updateInterval = 5000;

void action_index(EthernetClient client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();
  client.println(F("<!DOCTYPE HTML>"));
  client.println(F("<html>"));
  client.println(F("<head>"));
  client.println(F("<meta charset=\"utf-8\" />"));
  client.println(F("<title>Sensors server</title>"));
  client.println(F("</head>"));
  client.println(F("<body>"));
  client.println(F("<h1>Sensors server</h1><br>You could get <a href=\"/temp\">temperature</a>, <a href=\"/humidity\">humidity</a> or <a href=\"/co2\">CO<sub>2</sub></a> values."));
  client.println(F("</body>"));
  client.println(F("</html>"));
}

void action_temp(EthernetClient client) {
  update_sensors();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();
  client.print(F("Temperature: "));
  if (health_status) {
    client.print(temp_c);
  } else {
    client.print(F("nan"));
  }
  client.println(F(" C"));
}

void action_humidity(EthernetClient client) {
  update_sensors();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();
  client.print(F("Humidity: "));
  if (health_status) {
    client.print(humidity);
  } else {
    client.print(F("nan"));
  }
  client.println(F("%"));
}

void action_co2(EthernetClient client) {
  update_sensors();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();
  client.print(F("CO2: "));
  if (health_status) {
    client.print(co2_ppm);
  } else {
    client.print(F("nan"));
  }
  client.println(F(" ppm"));
}

void action_status_health(EthernetClient client) {
  if (health_status) {
    client.println(F("HTTP/1.1 200 OK"));
  } else {
    client.println(F("HTTP/1.1 500 Internal Server Error"));
  }
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();
  if (health_status) {
    client.println(F("OK"));
  } else {
    client.println(F("FAIL"));
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

void setup() {
  Serial.begin(9600);
  co2Serial.begin(9600);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print(F("Server started at http://"));
  Serial.print(Ethernet.localIP());
  Serial.println(F("/"));
  dht.begin();
  // initial delay for device stabilization
  delay(updateInterval);
  Serial.print("setup() free memory: ");
  Serial.println(freeMemory());
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // Limit request size to 1k
        if (request.length() < 128) {
          request += c;
        }
        if (c == '\n' && currentLineIsBlank) {
          Serial.println(request);
          if (request.indexOf("/temp") > 0) {
            action_temp(client);
            break;
          }
          if (request.indexOf("/humidity") > 0) {
            action_humidity(client);
            break;
          }
          if (request.indexOf("/co2") > 0) {
            action_co2(client);
            break;
          }
          if (request.indexOf("/status/health") > 0) {
            action_status_health(client);
            break;
          }
          // default response
          action_index(client);
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
    request = "";
    Serial.print("loop() free memory: ");
    Serial.println(freeMemory());
  }
}
