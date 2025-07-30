# ESP32-Cam-motion_sensor-Telegram
# 📷 Remote Control and Video Streaming via ESP32-CAM using Telegram Bot

## 🔧 Project Overview
A smart remote surveillance system based on the **ESP32-CAM**, controllable through a **Telegram Bot**, capable of:

- 📸 Sending a real-time photo
- 💡 Toggling the flash LED
- 🕵️‍♂️ Sending alerts upon motion detection (PIR sensor)
- 🌐 Retrieving IP address for live video streaming

> Built using the **Arduino IDE**, **Telegram Bot API**, and **Wi-Fi** connectivity.

---

## 📱 Telegram Bot Features

| Command    | Function                             |
|------------|--------------------------------------|
| `/start`   | Lists available commands             |
| `/photo`   | Captures and sends a real-time photo |
| `/flash`   | Toggles the flash LED                |
| `/IP`      | Sends the current device IP address  |

Motion detected via PIR sensor → Instant Telegram alert 🚨

---

## 🧰 Tools & Components

| Component         | Description                     |
|------------------|---------------------------------|
| ESP32-CAM         | Wi-Fi-enabled microcontroller    |
| PIR Sensor (HC-SR501) | Motion detection module     |
| FTDI Programmer (CP2102) | USB-to-Serial adapter   |
| Arduino IDE       | Programming environment         |
| Wi-Fi             | Wireless communication medium   |

---

## ⚙️ Setup Instructions

### 1. Install Arduino IDE  
[Download Arduino IDE](https://www.arduino.cc/en/software)

### 2. Add ESP32 Board URL  
`File` → `Preferences` → Add this URL: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json 
