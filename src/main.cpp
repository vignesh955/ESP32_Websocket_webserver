#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "IOT";
const char* password = "369369369";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Pin definitions
const int LED_PIN = 2;  // Built-in LED

bool ledState = false;
unsigned long startTime = 0;
uint8_t clientCount = 0;

// Web server on port 80
WebServer server(80);

// Simple HTML embedded in code as fallback
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
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 WebSocket Control</h1>
        <div id="status" class="status disconnected">Disconnected</div>
        
        <div>
            <button class="button btn-on" onclick="sendCommand('LED_ON')">LED ON</button>
            <button class="button btn-off" onclick="sendCommand('LED_OFF')">LED OFF</button>
            <button class="button btn-info" onclick="sendCommand('GET_STATUS')">Get Status</button>
        </div>
        
        <div id="systemInfo" style="margin: 20px 0; padding: 15px; background: #e9ecef; border-radius: 5px;">
            <strong>System Info:</strong><br>
            LED: <span id="ledStatus">Unknown</span><br>
            Heap: <span id="heap">-</span><br>
            Uptime: <span id="uptime">-</span>
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
                        document.getElementById('ledStatus').textContent = data.ledStatus ? 'ON' : 'OFF';
                        document.getElementById('ledStatus').style.color = data.ledStatus ? '#28a745' : '#dc3545';
                        document.getElementById('heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
                        document.getElementById('uptime').textContent = formatUptime(data.uptime);
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


void sendSystemInfo(uint8_t num) {
    DynamicJsonDocument doc(512);
    doc["type"] = "system_info";
    doc["ledStatus"] = ledState;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    doc["chipModel"] = ESP.getChipModel();
    
    String response;
    serializeJson(doc, response);
    webSocket.sendTXT(num, response);
}

void sendSystemInfoToAll() {
    DynamicJsonDocument doc(512);
    doc["type"] = "system_info";
    doc["ledStatus"] = ledState;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    
    String response;
    serializeJson(doc, response);
    webSocket.broadcastTXT(response);
}


void handleWebSocketMessage(uint8_t num, uint8_t * payload, size_t length) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
        String type = doc["type"];
        String command = doc["command"];
        
        if (type == "command") {
            if (command == "LED_ON") {
                ledState = true;
                digitalWrite(LED_PIN, HIGH);
                webSocket.sendTXT(num, "LED turned ON");
                Serial.println("LED turned ON");
                sendSystemInfoToAll();
                
            } else if (command == "LED_OFF") {
                ledState = false;
                digitalWrite(LED_PIN, LOW);
                webSocket.sendTXT(num, "LED turned OFF");
                Serial.println("LED turned OFF");
                sendSystemInfoToAll();
                
            } else if (command == "GET_STATUS") {
                sendSystemInfo(num);
                Serial.println("Status requested");
            }
        }
    } else {
        // If not JSON, echo as plain text
        String message = String((char*)payload);
        webSocket.sendTXT(num, "Echo: " + message);
    }
}

void handleRoot() {
    Serial.println("Serving embedded HTML page");
    server.send(200, "text/html", html_content);
}

void handleNotFound() {
    server.send(404, "text/plain", "File Not Found");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ESP32 WebSocket Server ===");
    
    // Initialize LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED pin initialized");
    
    // Try to initialize LittleFS (but don't fail if it doesn't work)
    Serial.println("Mounting LittleFS...");
    if (LittleFS.begin(true)) {
        Serial.println("LittleFS mounted successfully");
        
        // List files to see what's available
        Serial.println("Listing files:");
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
            Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
            file = root.openNextFile();
        }
        root.close();
    } else {
        Serial.println("LittleFS mount failed - using embedded HTML");
    }
    
    // Connect to WiFi
    Serial.printf("Connecting to WiFi: %s", ssid);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup HTTP routes
    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);
    
    // Start servers
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("HTTP server started on port 80");
    Serial.println("WebSocket server started on port 81");
    Serial.println("Open http://" + WiFi.localIP().toString() + " in your browser");
    
    startTime = millis();
}

void loop() {
    webSocket.loop();
    server.handleClient();
    
    // Broadcast system info every 10 seconds
    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast > 10000) {
        lastBroadcast = millis();
        sendSystemInfoToAll();
    }
    
    delay(10);
}