#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>

// Global variables
bool ledState = false; // Virtual LED state
const int LED_PIN = 2; // Built-in LED (GPIO2)
// uint32_t counter = 0;     // Counter variable

// WiFi credentials
const char *ssid = "IOT2";
const char *password = "369369369";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Global variables
uint32_t counter = 0;
unsigned long startTime = 0;
uint8_t clientCount = 0;
bool useFilesystem = false;

// Web server on port 80
WebServer server(80);

// Embedded HTML as fallback
const char html_content[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 WebSocket Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; font-weight: bold; }
        .connected { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .disconnected { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .button { padding: 12px 24px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
        .btn-on { background: #28a745; color: white; }
        .btn-off { background: #dc3545; color: white; }
        .btn-info { background: #17a2b8; color: white; }
        .messages { margin-top: 20px; padding: 10px; background: #f8f9fa; border-radius: 5px; max-height: 300px; overflow-y: auto; font-family: monospace; font-size: 12px; }
        .message { margin: 5px 0; padding: 5px; border-left: 3px solid #007bff; background: white; }
        .file-indicator { background: #ffc107; color: #856404; padding: 5px 10px; border-radius: 3px; font-size: 12px; margin-left: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 WebSocket Control
            <span class="file-indicator" id="fileIndicator">Embedded</span>
        </h1>
        <div id="status" class="status disconnected">Disconnected</div>

        <div>
            <button class="button btn-on" onclick="sendCommand('LED_ON')">LED ON</button>
            <button class="button btn-off" onclick="sendCommand('LED_OFF')">LED OFF</button>
            <button class="button btn-info" onclick="sendCommand('GET_STATUS')">Get Status</button>
            <button class="button" onclick="sendCommand('RESTART')" style="background: #6c757d; color: white;">Restart</button>
        </div>

        <div id="systemInfo" style="margin: 20px 0; padding: 15px; background: #e9ecef; border-radius: 5px;">
            <strong>System Info:</strong><br>
            LED: <span id="ledStatus">Unknown</span><br>
            Heap: <span id="heap">-</span><br>
            Uptime: <span id="uptime">-</span><br>
            Filesystem: <span id="fsStatus">Embedded HTML</span>
        </div>

        <div class="messages" id="messages"></div>
    </div>

    <script>
        var websocket;

        function initWebSocket() {
            var wsUri = "ws://" + window.location.hostname + ":81/";
            websocket = new WebSocket(wsUri);

            websocket.onopen = function(event) {
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').textContent = 'Connected to ESP32';
                addMessage('WebSocket connection opened');
            };

            websocket.onclose = function(event) {
                document.getElementById('status').className = 'status disconnected';
                document.getElementById('status').textContent = 'Disconnected - Reconnecting...';
                addMessage('WebSocket connection closed');
                setTimeout(initWebSocket, 2000);
            };

            websocket.onmessage = function(event) {
                addMessage('Received: ' + event.data);
                try {
                    var data = JSON.parse(event.data);
                    if (data.type === 'system_info') {
                        updateSystemInfo(data);
                    } else if (data.type === 'filesystem_status') {
                        document.getElementById('fsStatus').textContent = data.status;
                        document.getElementById('fileIndicator').textContent = data.usingFilesystem ? 'Filesystem' : 'Embedded';
                    }
                } catch(e) {
                    // Not JSON, check for LED status messages
                    if (event.data.includes('LED turned ON')) {
                        document.getElementById('ledStatus').textContent = 'ON';
                        document.getElementById('ledStatus').style.color = '#28a745';
                    } else if (event.data.includes('LED turned OFF')) {
                        document.getElementById('ledStatus').textContent = 'OFF';
                        document.getElementById('ledStatus').style.color = '#dc3545';
                    }
                }
            };

            websocket.onerror = function(event) {
                addMessage('WebSocket error: ' + event);
            };
        }

        function updateSystemInfo(data) {
            document.getElementById('ledStatus').textContent = data.ledStatus ? 'ON' : 'OFF';
            document.getElementById('ledStatus').style.color = data.ledStatus ? '#28a745' : '#dc3545';
            document.getElementById('heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
            document.getElementById('uptime').textContent = formatUptime(data.uptime);
        }

        function sendCommand(command) {
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                var message = JSON.stringify({
                    type: 'command',
                    command: command,
                    timestamp: Date.now()
                });
                websocket.send(message);
                addMessage('Sent: ' + command);
            } else {
                addMessage('WebSocket not connected');
            }
        }

        function addMessage(message) {
            var messagesDiv = document.getElementById('messages');
            var messageElement = document.createElement('div');
            messageElement.className = 'message';
            messageElement.textContent = new Date().toLocaleTimeString() + ': ' + message;
            messagesDiv.appendChild(messageElement);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        function formatUptime(seconds) {
            var hours = Math.floor(seconds / 3600);
            var minutes = Math.floor((seconds % 3600) / 60);
            var secs = seconds % 60;
            return hours + 'h ' + minutes + 'm ' + secs + 's';
        }

        // Initialize when page loads
        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

void sendSystemInfo(uint8_t num)
{
  DynamicJsonDocument doc(512);
  doc["type"] = "system_info";
  doc["ledStatus"] = ledState;
  doc["counter"] = counter;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  String response;
  serializeJson(doc, response);
  webSocket.sendTXT(num, response);
}

void sendSystemInfoToAll()
{
  DynamicJsonDocument doc(512);
  doc["type"] = "system_info";
  doc["ledStatus"] = ledState;
  doc["counter"] = counter; //
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
}

void sendFilesystemStatus(uint8_t num)
{
  DynamicJsonDocument doc(256);
  doc["type"] = "filesystem_status";
  doc["usingFilesystem"] = useFilesystem;
  doc["status"] = useFilesystem ? "LittleFS Active" : "Embedded HTML";

  String response;
  serializeJson(doc, response);
  webSocket.sendTXT(num, response);
}

void listAllFiles()
{
  Serial.println("=== LittleFS File Listing ===");
  File root = LittleFS.open("/");
  if (!root)
  {
    Serial.println("Failed to open root directory");
    return;
  }

  if (!root.isDirectory())
  {
    Serial.println("Root is not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  int fileCount = 0;

  while (file)
  {
    fileCount++;
    Serial.printf("  File: %s, Size: %d bytes\n", file.name(), file.size());
    file = root.openNextFile();
  }

  root.close();

  if (fileCount == 0)
  {
    Serial.println("  No files found in LittleFS");
  }
  else
  {
    Serial.printf("Total files: %d\n", fileCount);
    useFilesystem = true;
  }
  Serial.println("=============================");
}
void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length)
{
  String receivedPayload = String((char *)payload);
  Serial.println("Received WebSocket message: " + receivedPayload); // Debug lineF
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);

  if (!error)
  {
    String type = doc["type"];
    String command = doc["command"];
    Serial.println("Message type: " + type); // Debug line
    Serial.println("Command: " + command);   // Debug line
    if (type == "command")
    {
      if (command == "LED_ON")
      {
        ledState = true;
        // physically set built-in LED
        digitalWrite(LED_PIN, HIGH);
        webSocket.sendTXT(num, "LED turned ON");
        Serial.println("LED turned ON (GPIO2 HIGH)");
        sendSystemInfoToAll();
      }
      else if (command == "LED_OFF")
      {
        ledState = false;
        // physically clear built-in LED
        digitalWrite(LED_PIN, LOW);
        webSocket.sendTXT(num, "LED turned OFF");
        Serial.println("LED turned OFF (GPIO2 LOW)");
        sendSystemInfoToAll();
      }
      else if (command == "GET_STATUS")
      {
        sendSystemInfo(num);
        sendFilesystemStatus(num);
        Serial.println("Status requested");
      }
      else if (command == "RESTART")
      {
        webSocket.sendTXT(num, "Restarting ESP32...");
        delay(1000);
        ESP.restart();
      }
    }
  }
  else
  {
    // If not JSON, echo as plain text
    String message = String((char *)payload);
    webSocket.sendTXT(num, "Echo: " + message);
  }
}

void handleRoot()
{
  Serial.println("Serving web page");
  if (useFilesystem && LittleFS.exists("/index.html"))
  {
    Serial.println("Using filesystem index.html");
    File file = LittleFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  }
  else
  {
    Serial.println("Using embedded HTML");
    server.send(200, "text/html", html_content);
  }
}

void handleCSS()
{
  if (useFilesystem && LittleFS.exists("/style.css"))
  {
    File file = LittleFS.open("/style.css", "r");
    server.streamFile(file, "text/css");
    file.close();
    Serial.println("Served style.css from LittleFS");
  }
  else
  {
    server.send(404, "text/plain", "CSS not found");
    Serial.println("style.css not found in LittleFS");
  }
}

void handleJS()
{
  if (useFilesystem && LittleFS.exists("/script.js"))
  {
    File file = LittleFS.open("/script.js", "r");
    server.streamFile(file, "application/javascript");
    file.close();
    Serial.println("Served script.js from LittleFS");
  }
  else
  {
    server.send(404, "text/plain", "JS not found");
    Serial.println("script.js not found in LittleFS");
  }
}

void handleNotFound()
{
  server.send(404, "text/plain", "File Not Found: " + server.uri());
  Serial.println("404 Not Found: " + server.uri());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);
    clientCount = webSocket.connectedClients();
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());

    // Send welcome message
    String welcomeMsg = "Connected to ESP32. Client #" + String(num);
    webSocket.sendTXT(num, welcomeMsg);

    // Send initial status
    sendSystemInfo(num);
    sendFilesystemStatus(num);
    clientCount = webSocket.connectedClients();
  }
  break;

  case WStype_TEXT:
    Serial.printf("[%u] Received: %s\n", num, payload);
    handleWebSocketMessage(num, payload, length);
    break;

  case WStype_BIN:
    Serial.printf("[%u] Received binary length: %u\n", num, length);
    break;
  }
}

void setup()
{
  delay(5000);
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32 WebSocket Server ===");

  // Initialize built-in LED (GPIO2)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledState = false;
  Serial.println("Built-in LED (GPIO2) initialized and set LOW");

  // Initialize LittleFS
  Serial.println("Mounting LittleFS...");
  if (LittleFS.begin(true))
  {
    Serial.println("LittleFS mounted successfully");
    listAllFiles();
  }
  else
  {
    Serial.println("LittleFS mount failed");
    useFilesystem = false;
  }

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup HTTP routes
  server.on("/", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/script.js", handleJS);
  server.onNotFound(handleNotFound);

  // Start servers
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("HTTP server started on port 80");
  Serial.println("WebSocket server started on port 81");
  Serial.println("Ready! Open http://" + WiFi.localIP().toString() + " in your browser");

  startTime = millis();
}

void loop()
{
  webSocket.loop();
  server.handleClient();

  // Increment counter every second
  static unsigned long lastCount = 0;
  if (millis() - lastCount > 1000)
  {
    lastCount = millis();
    counter++;
  }

  // Broadcast system info every second (changed from 10 seconds to show real-time updates)
  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast > 1000)
  {
    lastBroadcast = millis();
    sendSystemInfoToAll();
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    WiFi.begin(ssid, password);
    delay(1000);
  }

  delay(10);
}