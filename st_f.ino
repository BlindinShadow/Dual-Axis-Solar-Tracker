#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// -----------------------
// WiFi Access Point Settings
// -----------------------
const char* apSSID     = "ESP32_AP";         // AP SSID
const char* apPassword = "yourAPpassword";   // AP password (min 8 characters)

// -----------------------
// Pin Definitions
// -----------------------
const int ldrLeftPin   = 34;  // ADC1 channel (0 = max light, 4095 = darkness)
const int ldrRightPin  = 35;
const int ldrTopPin    = 32;
const int ldrBottomPin = 33;
const int solarPin     = 36;  // ADC1 channel for solar panel voltage measurement

const int horizontalServoPin = 4;  // Horizontal servo on GPIO4
const int verticalServoPin   = 2;  // Vertical servo on GPIO2

// -----------------------
// Global Variables for Tracking (Solar Tracker Logic)
// -----------------------
int horizontalAngle = 90;
int verticalAngle   = 90;

int ldrLeftReading, ldrRightReading, ldrTopReading, ldrBottomReading;
int horizontalDifference, verticalDifference;
const int margin = 50; // Movement margin

int solarReading;
float solarVoltage;

// Create servo objects
Servo horizontalServo;
Servo verticalServo;

// -----------------------
// Web Server & HTML Content
// -----------------------
WebServer server(80);

// Latest HTML code stored in PROGMEM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Solar Tracker Dashboard</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      background-color: #e3f2fd;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
    }
    h1 {
      text-align: center;
      background-color: #0277bd;
      color: white;
      padding: 15px;
      border-radius: 8px;
    }
    .dashboard {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
    }
    .card {
      background-color: white;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
      padding: 20px;
      transition: transform 0.3s, box-shadow 0.3s;
    }
    .card:hover {
      transform: scale(1.05);
      box-shadow: 0 8px 16px rgba(0,0,0,0.3);
    }
    .readings {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .reading-box {
      padding: 15px;
      border-radius: 4px;
      text-align: center;
      color: white;
      font-weight: bold;
      transition: transform 0.3s;
    }
    .reading-box:hover {
      transform: scale(1.1);
    }
    .voltage {
      font-size: 24px;
      text-align: center;
      margin: 10px 0;
      color: #d84315;
      font-weight: bold;
    }
    button {
      background-color: #ff9800;
      color: white;
      border: none;
      padding: 10px;
      border-radius: 4px;
      cursor: pointer;
      width: 100%;
      margin-top: 10px;
      transition: transform 0.2s;
    }
    button:hover {
      background-color: #e65100;
      transform: scale(1.05);
    }
    /* Unique Colors for Readings */
    #ldr-left { background-color: #673ab7; }
    #ldr-right { background-color: #ff4081; }
    #ldr-top { background-color: #009688; }
    #ldr-bottom { background-color: #4caf50; }
    /* Circular Progress Bar Styles */
    .progress-circle {
      width: 120px;
      height: 120px;
      border-radius: 50%;
      background: conic-gradient(#ff9800 0deg, #ff9800 var(--progress), #e0e0e0 var(--progress), #e0e0e0 180deg);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 20px;
      font-weight: bold;
      color: #333;
      position: relative;
    }
    .progress-circle::before {
      content: '';
      width: 90px;
      height: 90px;
      border-radius: 50%;
      background-color: white;
      position: absolute;
    }
    .progress-circle span {
      position: relative;
      z-index: 1;
    }
    .progress-container {
      display: flex;
      justify-content: space-around;
      margin-top: 20px;
    }
    .progress-label {
      text-align: center;
      margin-top: 10px;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Solar Tracker</h1>
    <div class="dashboard">
      <div class="card">
        <h2>LDR Sensor Readings</h2>
        <div class="readings">
          <div id="ldr-left" class="reading-box">Left: 0</div>
          <div id="ldr-right" class="reading-box">Right: 0</div>
          <div id="ldr-top" class="reading-box">Top: 0</div>
          <div id="ldr-bottom" class="reading-box">Bottom: 0</div>
        </div>
      </div>
      <div class="card">
        <h2>Servo Positions</h2>
        <div class="progress-container">
          <div>
            <div id="horizontal-progress" class="progress-circle" style="--progress: 0deg;">
              <span id="horizontal-angle">0°</span>
            </div>
            <div class="progress-label">Horizontal</div>
          </div>
          <div>
            <div id="vertical-progress" class="progress-circle" style="--progress: 0deg;">
              <span id="vertical-angle">0°</span>
            </div>
            <div class="progress-label">Vertical</div>
          </div>
        </div>
      </div>
      <div class="card">
        <h2>Solar Panel Voltage</h2>
        <div class="voltage">
          <span id="solar-voltage">0.00</span> V
        </div>
      </div>
      <div class="card">
        <h2>Controls</h2>
        <button id="refresh-btn">Refresh Data</button>
      </div>
    </div>
  </div>
  <script>
    // Fetch data from ESP32 /data endpoint
    function fetchData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          // Update LDR readings
          document.getElementById('ldr-left').textContent = "Left: " + data.ldrLeft;
          document.getElementById('ldr-right').textContent = "Right: " + data.ldrRight;
          document.getElementById('ldr-top').textContent = "Top: " + data.ldrTop;
          document.getElementById('ldr-bottom').textContent = "Bottom: " + data.ldrBottom;
          // Update servo angle displays
          document.getElementById('horizontal-angle').textContent = data.horizontalAngle + "°";
          document.getElementById('vertical-angle').textContent = data.verticalAngle + "°";
          // Update circular progress bars (progress set to angle in degrees)
          document.getElementById('horizontal-progress').style.setProperty('--progress', data.horizontalAngle + "deg");
          document.getElementById('vertical-progress').style.setProperty('--progress', data.verticalAngle + "deg");
          // Update solar voltage
          document.getElementById('solar-voltage').textContent = data.solarVoltage;
        })
        .catch(error => console.error('Error fetching data:', error));
    }
    document.addEventListener('DOMContentLoaded', fetchData);
    document.getElementById('refresh-btn').addEventListener('click', fetchData);
    setInterval(fetchData, 5000);
  </script>
</body>
</html>
)rawliteral";

// -----------------------
// JSON Endpoint for Sensor and Servo Data
// -----------------------
void handleData() {
  // Note: LDR readings are intentionally swapped as in Code 1.
  String json = "{";
  json += "\"ldrLeft\":" + String(ldrRightReading) + ",";
  json += "\"ldrRight\":" + String(ldrLeftReading) + ",";
  json += "\"ldrTop\":" + String(ldrBottomReading) + ",";
  json += "\"ldrBottom\":" + String(ldrTopReading) + ",";
  json += "\"horizontalAngle\":" + String(horizontalAngle) + ",";
  json += "\"verticalAngle\":" + String(verticalAngle) + ",";
  json += "\"solarVoltage\":\"" + String(solarVoltage, 2) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// -----------------------
// Setup: Initialize Access Point, Web Server, and Servos
// -----------------------
void setup() {
  Serial.begin(115200);
  
  // Set up the ESP32 as an Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Define web server routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });
  server.on("/data", HTTP_GET, handleData);
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize servos
  horizontalServo.attach(horizontalServoPin);
  verticalServo.attach(verticalServoPin);
  horizontalServo.write(horizontalAngle);
  verticalServo.write(verticalAngle);
}

// -----------------------
// Main Loop: Solar Tracker Logic and Web Server Handling
// -----------------------
void loop() {
  // Handle incoming client requests
  server.handleClient();
  
  // --- Solar Tracker Logic (exactly as in Code 1) ---
  // Note: Sensor readings are intentionally swapped as in the original code.
  ldrLeftReading   = analogRead(ldrRightPin);
  ldrRightReading  = analogRead(ldrLeftPin);
  ldrTopReading    = analogRead(ldrBottomPin);
  ldrBottomReading = analogRead(ldrTopPin);
  
  horizontalDifference = ldrRightReading - ldrLeftReading;
  verticalDifference   = ldrTopReading - ldrBottomReading;
  
  if (horizontalDifference > margin && horizontalAngle < 180) {
    horizontalAngle += 5;
  } else if (horizontalDifference < -margin && horizontalAngle > 0) {
    horizontalAngle -= 5;
  }
  
  if (verticalDifference > margin && verticalAngle < 180) {
    verticalAngle += 5;
  } else if (verticalDifference < -margin && verticalAngle > 0) {
    verticalAngle -= 5;
  }
  
  horizontalServo.write(horizontalAngle);
  verticalServo.write(verticalAngle);
  
  // Read the solar panel voltage (assuming a 3.3V reference)
  solarReading = analogRead(solarPin);
  solarVoltage = solarReading * 3.3 / 4095.0;
  
  // Debug output to Serial Monitor
  Serial.print("Left: ");
  Serial.print(ldrLeftReading);
  Serial.print(" | Right: ");
  Serial.print(ldrRightReading);
  Serial.print(" | Top: ");
  Serial.print(ldrTopReading);
  Serial.print(" | Bottom: ");
  Serial.print(ldrBottomReading);
  Serial.print(" | Solar Voltage: ");
  Serial.println(solarVoltage);
  
  delay(100);
}
