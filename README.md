# TibS 🪴 Firmware

## Getting started

1. Install the latest ESP-IDF from [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step). (Tested using v5.1)
2. Clone this repository and navigate to the root directory.
3. Edit the WiFi credentials in `WiFi.h` to match your network.
4. Edit the URL in `WiFi.h` to match your server.
5. Compile and Upload the code to your ESP32 (C3).
6. Enjoy!

## How to install updates

1. Change the Version in "main.c" to your liking.
2. Compile the Code
3. Upload the '.bin' vie the Webinterface. **_Important note:_** The Version you enter on the Webinterface has to match the Version in the Code.
4. The firmware will be updated the next time the ESP32 connects to the server.
