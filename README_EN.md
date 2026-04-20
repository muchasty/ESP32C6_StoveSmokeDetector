# ESP32C6 StoveSmoke Detector 🍳🔥

An advanced, DIY kitchen safety sensor based on the **Seeed Studio XIAO ESP32-C6**. This device monitors flame presence, Volatile Organic Compounds (VOCs), temperature, and humidity, communicating natively via **Zigbee 3.0**.

## ✨ Key Features

* **Flame Detection:** Dual reading system using an IR flame sensor (Analog for intensity, Digital for instant interrupt-based alerts).
* **Digital Nose (SGP40):** High-quality VOC monitoring to detect smoke, burning food, or gas leaks.
* **Dynamic Alarm Threshold:** Adjust the SGP40 sensitivity on-the-fly via a slider in the Zigbee2MQTT dashboard (Range: 0-60000).
* **Environmental Monitoring:** Integrated SHTC3 sensor for precise temperature and humidity data.
* **Ultra-Low Latency:** Native Zigbee stack on ESP32-C6 ensures critical alerts are delivered instantly.
* **Multi-Endpoint Architecture:** 6 dedicated Zigbee endpoints for clean data representation in Home Assistant/OpenHAB.

## 🛠 Hardware Stack

1.  **MCU:** Seeed Studio XIAO ESP32-C6. https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/
2.  **VOC Sensor:** Sensirion SGP40 (I2C).
3.  **Temp/Hum Sensor:** DFRobot SHTC3 (I2C).
4.  **Flame Sensor:** Standard IR Flame sensor (A0 for Analog, D1 for Digital).
5.  **Board:** Designed for the **Uniboard MIDI** ecosystem.

## 📐 Zigbee Endpoints Map

The device exposes 6 logical endpoints to your coordinator:
- **EP1 (Illuminance):** Raw analog value from the flame sensor.
- **EP2 (Illuminance):** Raw VOC signal from the SGP40.
- **EP3 (Binary Input):** Flame Alarm status (ON/OFF).
- **EP4 (Binary Input):** Smoke/Smell Alarm status (based on custom threshold).
- **EP5 (Analog Output):** Control slider for the `sgp_threshold`.
- **EP6 (Temp/Hum):** Combined temperature and humidity sensor.

## 🚀 Getting Started

### 1. Arduino IDE Requirements
Ensure you have the ESP32 board package (v3.0+) installed. You will need the following libraries:
- `SensirionI2CSgp40`
- `DFRobot_SHTC3`
- `Zigbee` (included in the ESP32 core)

### 2. Zigbee2MQTT Configuration
This device requires an **External Converter**. 
1. Copy the `XIAO-C6-StoveSmoke.js` file to your Zigbee2MQTT directory.
2. Add it to your `configuration.yaml` under `external_converters`.
3. [Official Guide on External Converters](https://www.zigbee2mqtt.io/advanced/support-new-devices/01_support_new_devices.html)

**Pro-tip:** If temperature/humidity values show as `null` after the first pairing, click the **Reconfigure** button (yellow refresh icon) in the Zigbee2MQTT UI.

## 🔧 Operation

* **Pairing Mode:** Press and hold the button for 10 seconds to factory reset. The onboard LED will flash to indicate pairing mode.
* **Threshold Tuning:** Use the "SGP Alarm Threshold" slider in Z2M. Default is 20000. Increase the value if you get false positives; decrease it for higher sensitivity.
* **Burn-in Period:** The SGP40 sensor requires approx. 24-48 hours of continuous operation to stabilize its VOC baseline after assembly.

## 📄 License

This project is licensed under the MIT License - feel free to use and modify it for your smart home!
