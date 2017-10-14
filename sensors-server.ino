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
float humidity, temp_c, co2;

unsigned long currentMillis, previousMillis;
// update sensors every 5s
const long updateInterval = 5000;

void action_index(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<meta charset=\"utf-8\" />");
  client.println("<title>Sensors server</title>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1>Sensors server</h1><br>You could get <a href=\"/temp\">temperature</a>, <a href=\"/humidity\">humidity</a> or <a href=\"/co2\">CO<sub>2</sub></a> values.");
  client.println("</body>");
  client.println("</html>");
}

void action_temp(EthernetClient client) {
  update_sensors();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.print("Temperature: ");
  client.print(temp_c);
  client.println(" C");
}

void action_humidity(EthernetClient client) {
  update_sensors();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.print("Humidity: ");
  client.print(humidity);
  client.println("%");
}

void action_co2(EthernetClient client) {
  update_sensors();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.print("CO2: ");
  client.print(co2);
  client.println(" ppm");
}

void update_sensors() {
  currentMillis = millis();
  if (previousMillis > currentMillis) {
    previousMillis = 0;
    Serial.println("millis() overflow");
  }
  if (currentMillis - previousMillis >= updateInterval) {
    Serial.print("Refreshing sensors at ");
    Serial.print(currentMillis);
    Serial.println(" msec");
    previousMillis = currentMillis;

    humidity = dht.readHumidity();
    temp_c = dht.readTemperature();

    if (isnan(humidity) || isnan(temp_c)) {
      Serial.println("Failed to read data from DHT22");
      return;
    }

    // DHT22 measurements range
    // Humidity: 0..100%
    // Temperature: -40C..50C
    if (temp_c < -40.0 || temp_c >= 50.0) {
      Serial.println("Temperature value readed from DHT22 is out of range -40C .. 50C");
      return;
    }
    if (humidity < 0.0 || humidity > 100) {
      Serial.println("Humidity value readed from DHT22 is out of range 0%..100%");
      return;
    }

    co2 = readCO2();
    if (isnan(co2)) {
      Serial.println("Failed to read data from MH-Z19");
      return;
    }
  }
}

int readCO2()
{
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9];

  co2Serial.write(cmd, 9); //request PPM CO2
  co2Serial.readBytes(response, 9);

  for (int i = 0; i < 9; i++) {
    Serial.print("0x");
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println("");

  if (response[0] != 0xFF)
  {
    Serial.println("Incorrect start byte from MH-Z19 sensor");
    return -1;
  }

  if (response[1] != 0x86)
  {
    Serial.println("Incorrect response from MH-Z19 sensor");
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
  Serial.print("Server started at http://");
  Serial.print(Ethernet.localIP());
  Serial.println("/");
  dht.begin();
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // Limit request size to 1k
        if (request.length() < 1024) {
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
  }
}
