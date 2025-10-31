class ESP32WebSocketClient {
    constructor() {
        this.websocket = null;
        this.autoScroll = true;
        this.reconnectInterval = 2000;
        this.uptimeInterval = null;
        this.debug = true; // Enable debug logging
        this.init();
    }

    init() {
        this.initWebSocket();
        this.setupEventListeners();
        this.startUptimeUpdate();
    }

    initWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.hostname}:81/`;

        if (this.debug) console.log('Connecting to WebSocket:', wsUrl);

        this.websocket = new WebSocket(wsUrl);

        this.websocket.onopen = (event) => {
            if (this.debug) console.log('WebSocket connected');
            this.updateConnectionStatus(true);
            this.addMessage('WebSocket connection established', 'connected');
            // Request initial status
            this.sendCommand('GET_STATUS');
        };

        this.websocket.onclose = (event) => {
            this.updateConnectionStatus(false);
            this.addMessage('WebSocket connection closed. Attempting to reconnect...', 'error');
            setTimeout(() => this.initWebSocket(), this.reconnectInterval);
        };

        this.websocket.onmessage = (event) => {
            this.handleMessage(event.data);
        };

        this.websocket.onerror = (event) => {
            this.addMessage('WebSocket error occurred', 'error');
        };
    }

    handleMessage(data) {
        if (this.debug) console.log('Received WebSocket message:', data);
        this.addMessage(`Received: ${data}`, 'received');

        try {
            const jsonData = JSON.parse(data);
            this.processJsonMessage(jsonData);
        } catch (e) {
            if (this.debug) console.log('JSON parse error:', e);
            this.processTextMessage(data);
        }
    }
    sendCommand(command) {
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            const message = JSON.stringify({
                type: 'command',
                command: command,
                timestamp: Date.now()
            });

            if (this.debug) {
                console.log('Sending command:', message);
                console.log('WebSocket state:', this.websocket.readyState);
            }

            this.websocket.send(message);
            this.addMessage(`Sent: ${command}`, 'sent');
        } else {
            const state = this.websocket ?
                ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'][this.websocket.readyState] :
                'NULL';

            if (this.debug) {
                console.log('WebSocket not ready. State:', state);
            }
            this.addMessage(`WebSocket not connected (${state})`, 'error');
        }
    }

    processJsonMessage(data) {
        if (this.debug) console.log('Processing JSON message:', data);

        switch (data.type) {
            case 'status':
            case 'system_info':
                this.updateSystemInfo(data);
                break;
            case 'client_count':
                document.getElementById('client-count').textContent = data.count;
                break;
            default:
                if (this.debug) console.log('Unknown message type:', data.type);
                break;
        }
    }

    processTextMessage(message) {
        if (message.includes('LED turned ON')) {
            document.getElementById('led-status').textContent = 'ON';
            document.getElementById('led-status').style.color = '#2ecc71';
        } else if (message.includes('LED turned OFF')) {
            document.getElementById('led-status').textContent = 'OFF';
            document.getElementById('led-status').style.color = '#e74c3c';
        }
    }

    updateSystemInfo(data) {
        if (this.debug) console.log('Updating system info:', data);

        try {
            if (data.ledStatus !== undefined) {
                const ledStatus = document.getElementById('led-status');
                ledStatus.textContent = data.ledStatus ? 'ON' : 'OFF';
                ledStatus.style.color = data.ledStatus ? '#2ecc71' : '#e74c3c';
            }

            if (data.freeHeap !== undefined) {
                document.getElementById('free-heap').textContent =
                    `${(data.freeHeap / 1024).toFixed(1)} KB`;
            }

            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent =
                    this.formatUptime(data.uptime);
            }
        } catch (e) {
            if (this.debug) console.log('Error updating system info:', e);
        }
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;

        if (days > 0) {
            return `${days}d ${hours}h ${minutes}m`;
        } else if (hours > 0) {
            return `${hours}h ${minutes}m ${secs}s`;
        } else {
            return `${minutes}m ${secs}s`;
        }
    }

    // sendCommand(command) {
    //     if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
    //         const message = JSON.stringify({
    //             type: 'command',
    //             command: command,
    //             timestamp: Date.now()
    //         });

    //         this.websocket.send(message);
    //         this.addMessage(`Sent: ${command}`, 'sent');
    //     } else {
    //         this.addMessage('WebSocket not connected', 'error');
    //     }
    // }

    addMessage(message, type = 'info') {
        const messagesDiv = document.getElementById('messages');
        const messageElement = document.createElement('div');
        messageElement.className = `message ${type}`;

        const timestamp = new Date().toLocaleTimeString();
        messageElement.innerHTML = `
            <span style="color: #7f8c8d; font-size: 0.9em;">[${timestamp}]</span>
            ${message}
        `;

        messagesDiv.appendChild(messageElement);

        if (this.autoScroll) {
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }
    }

    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('status');
        if (connected) {
            statusElement.className = 'status connected';
            statusElement.innerHTML = `
                <span class="status-dot"></span>
                Connected to ESP32
            `;
        } else {
            statusElement.className = 'status disconnected';
            statusElement.innerHTML = `
                <span class="status-dot"></span>
                Disconnected - Reconnecting...
            `;
        }
    }

    clearMessages() {
        document.getElementById('messages').innerHTML = '';
    }

    toggleAutoScroll() {
        this.autoScroll = !this.autoScroll;
        const statusElement = document.getElementById('auto-scroll-status');
        statusElement.textContent = this.autoScroll ? 'ON' : 'OFF';
        statusElement.style.color = this.autoScroll ? '#2ecc71' : '#e74c3c';
    }

    startUptimeUpdate() {
        // This will be updated with real data from ESP32
        this.uptimeInterval = setInterval(() => {
            // Uptime will be updated via WebSocket messages
        }, 1000);
    }

    setupEventListeners() {
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey || e.metaKey) {
                switch (e.key) {
                    case '1':
                        e.preventDefault();
                        this.sendCommand('LED_ON');
                        break;
                    case '2':
                        e.preventDefault();
                        this.sendCommand('LED_OFF');
                        break;
                    case '3':
                        e.preventDefault();
                        this.sendCommand('GET_STATUS');
                        break;
                }
            }
        });
    }
}

// Add global error handler
window.addEventListener('error', (event) => {
    console.error('Global error:', event.error);
});

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    //window.esp32Client = new ESP32WebSocketClient();
    try {
        window.esp32Client = new ESP32WebSocketClient();
        console.log('ESP32 WebSocket Client initialized');
    } catch (e) {
        console.error('Failed to initialize ESP32 WebSocket Client:', e);
    }
});

// Export for global access
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ESP32WebSocketClient;
}

// Make global shortcuts for button onclick calls
function sendCommand(cmd) {
    if (window.esp32Client) {
        window.esp32Client.sendCommand(cmd);
    } else {
        console.error('ESP32 WebSocket client not initialized');
    }
}

function clearMessages() {
    if (window.esp32Client) window.esp32Client.clearMessages();
}

function toggleAutoScroll() {
    if (window.esp32Client) window.esp32Client.toggleAutoScroll();
}
