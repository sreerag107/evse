# ESP32 ATM90E26 Energy Meter

PlatformIO Arduino firmware for a NodeMCU-32S ESP32, with an ATM90E26 energy
meter and RFID, Wi-Fi, and NeoPixel support.

## Build

Open this folder as a PlatformIO project and run:

```sh
pio run
```

## Configuration and hardware

- Replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` in
  `src/sketch_oct29a.cpp` with local credentials before using Wi-Fi. Keep real
  credentials out of public commits.
- Check the pin assignments in the source against your actual board wiring.
- Calibrate the ATM90E26 for the installed meter module and verify its readings
  against a trusted meter before relying on them.
- Use a properly rated and isolated mains-measurement module. Never connect
  mains voltage directly to the ESP32.
