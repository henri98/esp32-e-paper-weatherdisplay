# esp32-e-paper-weatherdisplay
[![Build Status](https://travis-ci.com/henri98/esp32-e-paper-weatherdisplay.svg?branch=master)](https://travis-ci.com/henri98/esp32-e-paper-weatherdisplay) ![](https://img.shields.io/github/stars/henri98/esp32-e-paper-weatherdisplay.svg) ![](https://img.shields.io/github/license/henri98/esp32-e-paper-weatherdisplay.svg)

An ESP32 and 4.2" ePaper Display reads Dark Sky weather API and displays the weather using the [Espressif IoT Development Framework](https://github.com/espressif/esp-idf)

![esp32-e-paper-weatherdisplay-0](https://user-images.githubusercontent.com/9615443/50996018-24fa5380-1521-11e9-8491-38f05efca19d.gif)

## Getting Started

### Hardware

For this project an ESP32 development module and a Waveshare 4.2inch e-Paper module are used. Documentation about this e-Paper module can be found [here](https://www.waveshare.com/wiki/4.2inch_e-Paper_Module). As ESP32 development board the DOIT ESP32 DEVKIT V1 can be used used.

To save battery power, the SILABS CP210x USB to UART Bridge and the AMS1117 voltage regulator can be removed from the development board. An external USB to UART converter can be used for programming. 

#### Pin configuration

| Waveshare e-Paper | DOIT ESP32 DEVKIT V1 |
| ----------------- | -------------------- |
| MOSI_PIN          | GPIO_NUM_27          |
| CLK_PIN           | GPIO_NUM_26          |
| CS_PIN            | GPIO_NUM_25          |
| DC_PIN            | GPIO_NUM_33          |
| RST_PIN           | GPIO_NUM_32          |
| BUSY_PIN          | GPIO_NUM_35          |
| VCC               | 3V3                  |
| GND               | GND                  |

| Update Button     | DOIT ESP32 DEVKIT V1 |
| ----------------- | -------------------- |
| Pin A             | GPIO_NUM_4 PIN       |
| Pin B             | GND                  |

| Reset Button      | DOIT ESP32 DEVKIT V1 |
| ----------------- | -------------------- |
| Pin A             | RESET PIN            |
| Pin B             | GND                  |
            
Connect a LiPo battery to the VCC of the Waveshare e-Paper module.


### Software

#### Prerequisites
Please check [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for getting started instructions.

#### Clone the project 

```bash
git clone https://github.com/henri98/esp32-e-paper-weatherdisplay.git
```

#### Build and flash the project 

Navigate to the project folder:

```bash
cd esp32-e-paper-weatherdisplay
```

Configure the project:

```bash
make menuconfig 
```
The configuration menu will be displayed. Navigate to WiFi Configuration and enter credentials to connect to the WiFi. Navigate to Dark Sky configuration and configure the Latitude, Longitude and the Dark Sky API key. You can get an API key [here](https://darksky.net/dev). With an API key you can make up to 1000 free requests, so enough to keep the weather display up to date.  

Build and flash the firmware on the ESP32:

```bash
make flash 
```
To monitor the programm use:
```bash
make monitor 
```

You can also choose to update the firmware over the air. This requires that a previous version of the firmware has already flashed. The update will be downloaded over HTTP. The update URL can be configured:

```bash
make menuconfig
```
The configuration menu will be displayed. Navigate to WiFi Configuration and enter the URL where the firmware can be found. After compiling the binary file can be found in the build directory: e-paper-weatherdisplay.bin

A simple HTTP server can be created using python:

```bash
python3 -m http.server 8080
```

If you start the HTTP server in the root of the project, the firmware can be retrieved using:

http://ip:8080/build/e-paper-weatherdisplay.bin

By pressing the update button (connect pin 4 to GND) after a reset the update will be started.


## Casing 

A case has been made for the hardware. This can be found on Thingiverse: https://www.thingiverse.com/thing:3357579

## License

This source of this project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Icons and fonts

The weather icon images used are from https://github.com/erikflowers/weather-icons. The images are licensed under [SIL OFL 1.1](http://scripts.sil.org/OFL). The font used is the Ubuntu font, the license can be found [here](https://www.ubuntu.com/legal/font-licence).

The images and font are converted to "C" source format using [LCD Image Converter](https://github.com/riuson/lcd-image-converter). 


## Images 

![esp32-e-paper-weatherdisplay-1](https://user-images.githubusercontent.com/9615443/50922221-40465f80-144a-11e9-85fb-8d3b429d94a6.jpeg)

![esp32-e-paper-weatherdisplay-2](https://user-images.githubusercontent.com/9615443/50922222-40465f80-144a-11e9-8928-c48453101c8a.jpeg)
