# TibS 🪴 Firmware

## Getting started

1. Install the latest ESP-IDF from [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step). (Tested using v5.1)
2. Clone this repository and navigate to the root directory.
3. Edit the WiFi credentials in `WiFi.h` to match your network.
4. Edit the URL in `WiFi.h` to match your server.
5. Compile and Upload the code to your ESP32 (C3).

   - To do so, connect the PCB using the integrated JTAG interface.
     | USB | PCB |
     |-----|-----|
     | VCC | 2 |
     | D+ | 7 |
     | D- | 8 |
     | GND | 14 |
   - You might have to put the Controller into download mode. To do so, hold the BOOT_SELECT Button, while pressing the RESET Button. Then release the RESET Button and then the BOOT_SELECT Button.
6. If this is the first time connecting, you will have to enter the WiFi credentials.

   1. Connect to the WiFi network `TibSense`
   2. Open a browser and navigate to `192.168.4.1`
   3. Enter the WiFi credentials and click `Save`

7. Enjoy!

## How to install updates

1. Change the Version in `main.c` to your liking.
2. Compile the Code
3. Upload the `.bin` vie the Webinterface.
   **_Important note:_** The Version you enter on the Webinterface has to match the Version in the Code.
4. The firmware will be updated the next time the ESP32 connects to the server.
