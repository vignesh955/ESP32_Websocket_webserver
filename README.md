# ESP32 WebSocket LED Control

A web-based LED control system using ESP32 with WebSocket communication and filesystem support.

## Project Structure
```
ESP32_websocket_webserver/
├── data/                     # Web files directory
│   ├── index.html           # Main webpage
│   ├── script.js            # JavaScript code
│   └── style.css            # CSS styles
├── src/
│   └── main.cpp             # Main application code
├── partitions.csv           # Custom partition layout
└── platformio.ini           # PlatformIO configuration
```
 **Initialize LittleFS**
   - Create the `data` directory
   - Place web files (index.html, script.js, style.css) in `data` directory

4. **Build and Upload**
   ```bash
   # Build project
   pio run
   # build the filesystem
   pio run --target buildfs or platformio icon in the left bar -> Platform -> Build Filesystem Image

   # Upload filesystem
   pio run --target uploadfs  or platformio icon in the left bar -> Platform -> Upload Filesystem Image

   # Upload firmware
   pio run -t upload
   ```
