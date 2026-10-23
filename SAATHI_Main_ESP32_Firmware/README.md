# SAATHI Main ESP32 Firmware

## Install libraries
Arduino IDE > Sketch > Include Library > Manage Libraries:
- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- ThingSpeak by MathWorks

Boards Manager: install `esp32 by Espressif Systems`.

## Add private credentials
1. Copy `secrets.h.example` to `secrets.h` in the same folder as `SAATHI_Main_ESP32.ino`.
2. Replace the password and ThingSpeak write-key placeholders in `secrets.h`.
3. Do not publish `secrets.h`.

## Upload
1. Open `SAATHI_Main_ESP32.ino`.
2. Select `ESP32 Dev Module` and the correct COM port.
3. Verify, upload, and open Serial Monitor at 115200 baud.
4. Read the IP address printed by the ESP32.

## Relay warning
Relay polarity is unknown. Code defaults to active-LOW. With the pump disconnected, test whether relay input LOW means ON. If your relay is active-HIGH, set `RELAY_ACTIVE_LOW = false` and retest. Do not connect the pump until relay OFF/ON behavior is verified.

## Soil calibration
`SOIL_DRY_THRESHOLD=2800` and `SOIL_WET_THRESHOLD=2300` are placeholders only. Read `SoilRaw` in dry and wet soil, check whether dry soil gives a higher or lower number, then update the thresholds and comparison logic if necessary. Test AUTO with the pump disconnected first.

## Endpoints
- `/ON` — pump on; exits AUTO
- `/OFF` — pump off; exits AUTO
- `/STATUS` — JSON status and sensor readings
- `/AUTO` — enables automatic mode
- `/MANUAL` — disables automatic mode

## ThingSpeak fields
1 temperature °C; 2 humidity %; 3 raw soil ADC; 4 pump status (1 on, 0 off).
Writes occur every 20 seconds; respect the update limits shown in your ThingSpeak account.
