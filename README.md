# esp32-e-paper-weatherdisplay
[![Build Status](https://travis-ci.com/henri98/esp32-e-paper-weatherdisplay.svg?branch=master)](https://travis-ci.com/henri98/esp32-e-paper-weatherdisplay) ![](https://img.shields.io/github/stars/henri98/esp32-e-paper-weatherdisplay.svg) ![](https://img.shields.io/github/license/henri98/esp32-e-paper-weatherdisplay.svg)

An ESP32 and 4.2" ePaper Display reads Dark Sky weather API and displays the weather using the [Espressif IoT Development Framework](https://github.com/espressif/esp-idf)

## Getting Started

### Prerequisites
Please check [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for getting started instructions.

### Clone the project 

```bash
git clone https://github.com/henri98/esp32-e-paper-weatherdisplay.git
```

### Build the project 

Navigate to the project folder:

```bash
cd esp32-e-paper-weatherdisplay
```

Configure the project:

```bash
make menuconfig 
```
The configuration menu will be displayed. Navigate to WiFi Configuration and enter credentials to connect to the WiFi. Navigate to Dark Sky configuration and configure the Latitude, Longitude and the Dark Sky API key. You can get an API key [here](https://darksky.net/dev). With an API key you can make up to 1000 free requests, so enough to keep the weather display up to date.  

Build and flash the firmware on the esp32:

```bash
make flash 
```
To monitor the programm use:
```bash
make monitor 
```

## License

This source of this project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Icons and fonts

The weather icon images used are from https://github.com/erikflowers/weather-icons. The images are licensed under [SIL OFL 1.1](http://scripts.sil.org/OFL). The font used is the Ubuntu font, the license can be found [here](https://www.ubuntu.com/legal/font-licence).

The images and font are converted to "C" source format using [LCD Image Converter](https://github.com/riuson/lcd-image-converter). 


## Images 

![](https://user-images.githubusercontent.com/9615443/50922221-40465f80-144a-11e9-85fb-8d3b429d94a6.jpeg)

![](https://user-images.githubusercontent.com/9615443/50922222-40465f80-144a-11e9-8928-c48453101c8a.jpeg)
