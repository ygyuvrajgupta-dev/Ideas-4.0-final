# SAATHI Smart Agriculture Dashboard

## Setup
1. Extract this folder.
2. Open `index.html`, or run `python -m http.server 5500` in this folder and visit `http://localhost:5500`.
3. Click Settings and enter the main ESP32 controller base URL shown in its Serial Monitor.
4. Camera stream defaults to `http://10.122.243.180:81/stream`. Change it if the camera IP changes.
5. Enter ThingSpeak channel ID and Read API key if the channel is private. Default fields: 1 temperature, 2 humidity, 3 soil ADC, 4 pump status.

## Main ESP32 firmware API expected
- `GET /STATUS` returns JSON properties `pump`, `pumpStatus`, `autoMode`, `temperature`, `humidity`, `soilRaw`.
- `GET /ON`, `/OFF`, `/AUTO`, `/MANUAL`.
- Firmware needs CORS headers for requests from this dashboard.

The main ESP32 firmware sends data to ThingSpeak; this website reads the latest entry. It does not upload sensor data itself. Browser settings are local only, but a static website cannot keep API keys secret from someone who can inspect the browser. Do not commit real keys or passwords to a public repository. Both boards and the device running this website must be reachable on the same LAN/Wi-Fi.
