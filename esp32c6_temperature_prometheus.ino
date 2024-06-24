/*
 Arduino ESP32C6 WiFi Web Server for the built in temperature sensor.
 Responds to http requests with prometheus.io syntax responses.

 # HELP ambient_temperature Ambient temperature
 # TYPE ambient_temperature gauge
 ambient_temperature 31.00
 # HELP temperature Sensor temperature
 # TYPE temperature gauge
 temperature 46.00

 Based on:
 * ESP32 example code SimpleWiFiServer by Jan Hendrik Berlin

 Written by Simon Loffler
*/

#include "secrets.h"

// Uncomment if you'd like to use the active NFC module: ST25DV16K
// #define NFC

// If on an ESP32-C3 set this
// int LED_BUILTIN = 13;

// Task scheduler
#include <TaskScheduler.h>
void readSensorCallback();
Task readSensorTask(5000, -1, &readSensorCallback);  // Read sensor every 5 seconds
Scheduler runner;

#include <Arduino.h>
#include <Wire.h>

#ifdef NFC
  // NFC board
  #include "ST25DVSensor.h"
  ST25DV st25dv(12, -1, &Wire);
#endif

float temperature;
String ipAddress;
int temperatureCompensation = 13;

// Task callback
void readSensorCallback() {
    // Read temperature
    temperature = temperatureRead();
    printToSerial((String)"Temperature: " + temperature + "C");

    #ifdef NFC
      // Write NFC
      String tempStr = String(temperature, 0); // Convert float to String with 2 decimal places
      String uriMessage = ipAddress + "/" + tempStr; // Concatenate strings
      const char* uri_write_message = uriMessage.c_str(); // Get C-style string
      const char uri_write_protocol[] = URI_ID_0x01_STRING;
      if (st25dv.writeURI(uri_write_protocol, uri_write_message, "")) {
        printToSerial("Write failed!");
        while(1);
      } else {
        printToSerial((String)ipAddress + "/" + temperature);
      }
    #endif

    // Pulse blue LED
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
}

void printToSerial(String message) {
  // Check for Serial to avoid the ESP32-C3 hanging attempting to Serial.print
  if (Serial) {
    Serial.println(message);
  }
}

// WiFi init
#include <WiFi.h>
char* ssid     = SECRET_SSID;
char* password = SECRET_PASSWORD;
WiFiServer server(80);

void setup() {

    Serial.begin(115200);
    delay(100);

    pinMode(LED_BUILTIN, OUTPUT);

    // Sensor setup
    Wire.begin();

    #ifdef NFC
      // NFC setup
      if(st25dv.begin() == 0) {
        printToSerial("NFC Init done!");
      } else {
        printToSerial("NFC Init failed!");
        while(1);
      }
    #endif

    // Setup read sensor task
    readSensorTask.enable();
    runner.addTask(readSensorTask);
    runner.enableAll();

    // WiFi setup
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);

    delay(10);

    // Set WiFi power
    // Max: WIFI_POWER_19_5dBm ~150mA
    // Min: WIFI_POWER_MINUS_1dBm ~120mA
    // WiFi.setTxPower(WIFI_POWER_2dBm);

    // We start by connecting to a WiFi network
    printToSerial((String)"Connecting to " + ssid);

    WiFi.begin(ssid, password);

    // Wait for a WiFi connection for up to 10 seconds
    for (int i = 0; i < 10; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        printToSerial(".");
        delay(500);
      } else {
        printToSerial("WiFi connected.");
        printToSerial("IP address: ");
        ipAddress = (String)WiFi.localIP().toString();
        printToSerial(ipAddress);

        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        break;
      }
    }
    server.begin();
}

void loop() {
  runner.execute();

  // WiFi server
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    printToSerial("New Client.");           // print a message out the serial port
    printToSerial("");
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        // printToSerial("" + c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=UTF-8");
            client.println();

            // Pulse the LED to show a connection has been made
            digitalWrite(LED_BUILTIN, HIGH);

            // Send Prometheus data
            client.print("# HELP ambient_temperature Ambient temperature\n");
            client.print("# TYPE ambient_temperature gauge\n");
            client.print((String)"ambient_temperature " + (temperature - temperatureCompensation) + "\n");
            client.print("# HELP temperature Sensor temperature\n");
            client.print("# TYPE temperature gauge\n");
            client.print((String)"temperature " + temperature + "\n");

            digitalWrite(LED_BUILTIN, LOW);

            // The HTTP response ends with another blank line:
            client.print("\n");
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    printToSerial("Client Disconnected.");
  }
}
