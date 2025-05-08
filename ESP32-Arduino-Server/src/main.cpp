#include <WiFi.h>
#include "driver/ledc.h"
#include "driver/adc.h"

// Wi-Fi credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

// PWM configuration
const int pwmChannel = 0;
const int pwmFreq = 20000;       // 20 kHz
const int pwmResolution = 8;     // 8-bit resolution (0–255)
const int pwmPin = 33;

// Web server
WiFiServer server(80);
String header;
int pwmValue = 0;

int val = 0;

TaskHandle_t Task1;

void Task1code(void * parameter) {
  // Setup Access Point
  Serial.println("Setting AP (Access Point)...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();

  for (;;) {
    WiFiClient client = server.available();
    if (client) {
      Serial.println("New Client.");
      String currentLine = "";
      header = "";

      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          header += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {

              // Handle AJAX request for analog value
              if (header.indexOf("GET /val") >= 0) {
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/plain");
                client.println("Connection: close");
                client.println();
                client.println(val);  // Return analog value
                break;
              }

              // Parse GET request for PWM value
              if (header.indexOf("GET /?value=") >= 0) {
                int start = header.indexOf("value=") + 6;
                int end = header.indexOf(" ", start);
                String valueStr = header.substring(start, end);
                pwmValue = constrain(valueStr.toInt(), 0, 255);
                Serial.print("Received PWM value: ");
                Serial.println(pwmValue);
                ledcWrite(pwmChannel, pwmValue);
              }

              // Respond with web page
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<title>ESP32 PWM Control</title></head>");
              client.println("<body><h1>ESP32 PWM Control</h1>");
              client.println("<p>Current PWM Value: " + String(pwmValue) + "</p>");
              client.println("<p>Current Analog Read Value: <span id=\"val\">Loading...</span></p>");
              client.println("<form action=\"/\" method=\"GET\">");
              client.println("Set PWM (0 to 255): <input type=\"number\" name=\"value\" min=\"0\" max=\"255\" required>");
              client.println("<input type=\"submit\" value=\"Set PWM\"></form>");

              // JavaScript to update analog value every second
              client.println("<script>");
              client.println("setInterval(function() {");
              client.println("  fetch('/val').then(response => response.text()).then(data => {");
              client.println("    document.getElementById('val').innerText = data;");
              client.println("  });");
              client.println("}, 1000);");
              client.println("</script>");

              client.println("</body></html>");
              client.println();
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
      }
      client.stop();
      Serial.println("Client disconnected.");
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Configure ADC
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_12);

  // Setup PWM on pin 33
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
  ledcWrite(pwmChannel, pwmValue);

  // Start web server task on core 0
  xTaskCreatePinnedToCore(
    Task1code,  // Function
    "Task1",    // Name
    10000,      // Stack size
    NULL,       // Parameter
    0,          // Priority
    &Task1,     // Task handle
    0);         // Core
}

void loop() {
  // Read analog input (averaged)
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += adc1_get_raw(ADC1_CHANNEL_3);
  }
  val = sum / 10;

  Serial.println(val);
  delay(1000);
}
