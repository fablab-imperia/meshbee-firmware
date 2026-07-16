# 🐝 MeshBee - Sistema di Monitoraggio Arnie

Codice ESP32 per sensori

meshbee-firmware/
├── receiver/ # Firmware ricevitore (gateway MQTT)
├── sender/   # Firmware trasmettitore (sensori)
└── HX711/    # Libreria celle di carico

### Prerequisiti

```bash
Arduino IDE >= 2.0
ESP32 Board Support
Librerie: HX711, DHT, PubSubClient
```

### Hardware ESP32

**a) Configura le credenziali**

Crea il file `receiver/credentials.h` e sostituisci i placeholder `xxx` con i valori reali:

```c
const char* mqtt_server = "192.168.1.100";  // IP del tuo server
const char* user        = "beehive";        // valore di MQTT_USER in .env
const char* pass        = "...";            // valore di MQTT_PASSWORD in .env

const char* ssid        = "NomeRete";       // SSID Wi-Fi
const char* password    = "...";            // password Wi-Fi
```

> ⚠️ Non committare `credentials.h` dopo aver inserito le credenziali reali. Usa il file `credentials.h.example` come riferimento.

**b) Carica il firmware**

1. Apri Arduino IDE 2.x
2. Installa il board support per ESP32 (Boards Manager → `esp32 by Espressif`)
3. Installa le librerie: `HX711`, `DHT sensor library`, `PubSubClient`, `ArduinoJson`
4. Apri il file `.ino` corretto (`sender/sender.ino` o `receiver/receiver.ino`)
5. Seleziona la board `ESP32 Dev Module` e la porta COM/USB corretta
6. Clicca **Upload**


## 🌟 Features

### Hardware

✅ Sensori temperatura/umidità
✅ Misurazione peso arnia (4 celle carico)
✅ Trasmissione MQTT via WiFi
✅ Low power mode
✅ Alimentazione solare

## 📄 Licenza

Questo progetto è distribuito sotto licenza GPLv3.

## 👥 Team

Sviluppato per apicoltori e monitoraggio ambientale.

---
**Fatto con ❤️ per le api** 🐝
