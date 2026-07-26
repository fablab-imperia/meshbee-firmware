#define DEBUG 1                                  // SET TO 0 OUT TO REMOVE TRACES

#define NoSensors                 8              // number of sensors - max 8
#define NoTransmission            4              // number of transmission via meshtastic
#define SoftwareVersion "10-06-2026"

#define DELAYTIME               250
#define TIME_TO_SLEEP            60              // default sleep time in minutes

const char* ssid     = "MeshBee";
const char* password = "MeshBee123";

#if DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__);
#define D_print(...)       Serial.print(__VA_ARGS__)
#define D_write(...)       Serial.write(__VA_ARGS__)
#define D_println(...)     Serial.println(__VA_ARGS__)
#define D_printf(...)      Serial.printf(__VA_ARGS__)
#define M_print(...)       meshSerial.print(__VA_ARGS__)
#define M_println(...)     meshSerial.println(__VA_ARGS__)
#define W_print(...)       WebSerial.print(__VA_ARGS__)
#define W_println(...)     WebSerial.println(__VA_ARGS__)
#else
#define D_SerialBegin(...)
#define D_print(...)
#define D_write(...)
#define D_println(...)
#define D_printf(...) 
#define M_print(...) 
#define M_println(...)
#define W_print(...)
#define W_println(...)
#endif
