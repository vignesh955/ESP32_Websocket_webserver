class ESP32WebSocketClient {
    constructor() {
        this.websocket = null;
        this.autoScroll = true;
        this.reconnectInterval = 2000;
        this.uptimeInterval = null;
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
        
        this.websocket = new WebSocket(wsUrl);
        
        this.websocket.onopen = (event) => {
            this.updateConnectionStatus(true);
            this.addMessage('WebSocket connection established', 'connected');
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
        this.addMessage(`Received: ${data}`, 'received');
        
        try {
            const jsonData = JSON.parse(data);
            this.processJsonMessage(jsonData);
        } catch (e) {
            // Not JSON, process as plain text
            this.processTextMessage(data);
        }
    }

    processJsonMessage(data) {
        switch(data.type) {
            case 'status':
                this.updateSystemInfo(data);
                break;
            case 'system_info':
                this.updateSystemInfo(data);
                break;
            case 'client_count':
                document.getElementById('client-count').textContent = data.count;
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

    sendCommand(command) {
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            const message = JSON.stringify({
                type: 'command',
                command: command,
                timestamp: Date.now()
            });
            
            this.websocket.send(message);
            this.addMessage(`Sent: ${command}`, 'sent');
        } else {
            this.addMessage('WebSocket not connected', 'error');
        }
    }

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
                switch(e.key) {
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

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.esp32Client = new ESP32WebSocketClient();
});

// Export for global access
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ESP32WebSocketClient;
}