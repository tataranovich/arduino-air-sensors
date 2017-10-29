# Air sensors

Temperature, relative humidity and carbon dioxide sensors using DHT22 and MH-Z19 on top of ESP8266 controller.

## API

In this example 192.168.1.177 is an IP address which WiFi access point assigned to ESP8266 board. Actual IP address printed to serial port during boot.

- http://192.168.1.177/ - main page.
- http://192.168.1.177/temp - get temperature readings (degrees celsium).
- http://192.168.1.177/humidity - get relative humidity readings (%).
- http://192.168.1.177/co2 - get carbon dioxide readings (ppm).
- http://192.168.1.177/status/health - internal health check. Could be monitored by tools like Nagios.
