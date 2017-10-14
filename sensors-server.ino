#include <SPI.h>
#include <Ethernet.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,177);
EthernetServer server(80);
String request;

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
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("Temperature: 23.20 C");
}

void action_humidity(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("Humidity: 37.00%");
}

void action_co2(EthernetClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("CO2: 870 ppm");
}

void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("Server started at http://");
  Serial.print(Ethernet.localIP());
  Serial.println("/");
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
