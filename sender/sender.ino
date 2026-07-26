
//https://github.com/pangcrd/LVGL_Bassic-tutorial/tree/main/ESP32_UART_JSON/Source%20code/Master

#include "config.h"

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define WAKEUP_GPIO              GPIO_NUM_33     // Only RTC IO are allowed - ESP32 Pin example
#define ALARM_GPIO               GPIO_NUM_32     // ESP32 GPIO: 0, 2, 4, 12-15, 25-27, 32-39;

#include <Arduino.h>
#include "ArduinoJson.h"
#include "soc/rtc.h"   // set CPU frequency

#include <WiFi.h>
#include "esp_wifi.h"

//#include "esp_sleep.h"   // ????????????????????????????????????? wofür brauche ich die ?
// https://github.com/pycom/pycom-esp-idf/blob/master/components/esp32/include/esp_sleep.h

// for non volatile storage
#include <Preferences.h>
#include <nvs_flash.h>              // non volatile storage library

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <ElegantOTA.h>

Preferences preferences;


//*************************
//****   interrupt      ***
//*************************


volatile bool pressed = false;

// For debouncing the pushbutton
const unsigned long DEBOUNCE_DELAY = 100;  // in milliseconds
volatile unsigned long lastPressTime = 0;

// Interrupt Service Routine (ISR)
void ARDUINO_ISR_ATTR buttonISR() {
  unsigned long now = millis();
  if (now - lastPressTime > DEBOUNCE_DELAY) {
    pressed = true;
  }
  lastPressTime = now;
}


//*************************
//****   cpu frequency  ***
//*************************

void setCPUfreq( int freq ) {                // 40 or 240 MHz
  setCpuFrequencyMhz(freq);
  D_print("set cpu frequency to: ");
  D_print(getCpuFrequencyMhz());
  D_println(" MHz");
}
  
//*************************
//**** HX711 load cells ***
//*************************


#include "HX711r.h"

HX711 scale;

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN  = 25;


//********************
//**** Meshtastic ****
//********************


#define MESH_BAUD 115200

// Define the RX and TX pins for Serial 2 to Meshtastic
#define RXD2 16
#define TXD2 17

// Create an instance of the HardwareSerial class for Serial 2
HardwareSerial meshSerial(2);

String receivedMessage = "";  // Variable to store the complete message


//********************
//**** deep sleep ****
//********************


#define uS_TO_S_FACTOR           1000000ULL      // Conversion factor for micro seconds to seconds (ULL = unsigned long long)
#define OSC_TOLERANCE            0.5             // adjustment for oscillator deviation
#define TIME_TO_BE_AWAKE         20              // Time ESP32 will stay awake (in seconds)
uint32_t wake_time  = 0;
int      sleep_time;


//*******************
//****    OTA     ***
//*******************


unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}


//*******************
//**** DHT sensor ***
//*******************


#include "DHT.h"
#include <DHT_U.h>

#define DHTPIN 4     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);


//*********************
//**** Multiplexer ****
//*********************


const int SPin[4] = {18, 23, 19, 22}; // S0, S1, S2, S3 / 4 Pin usati per inviare il codice binario

// const int EPin = 2; /* Pin Enable
//                      - if set to HIGH physically interrupted fisicamente the connection between pin SIG the chosen Yxx pin
//                      - if set to LOW connection established between pin SIG and the chosen Yxx pin */
// fixed connected to GND

// const int SIG = 4; // data pin already define as DHTPIN in DHT22 section and as LOADCELL_DOUT_PIN in HX711 section

const int STable[16][4] = {
  // Creates an Array with the binary values to recall in base of the chosen channel Y
  // s0, s1, s2, s3, canale
  {0,  0,  0,  0}, // Y0  sensor 1 temperature, humidity
  {1,  0,  0,  0}, // Y1  sensor 1 weight
  {0,  1,  0,  0}, // Y2  sensor 2 temperature, humidity
  {1,  1,  0,  0}, // Y3  sensor 2 weight
  {0,  0,  1,  0}, // Y4  sensor 3 temperature, humidity
  {1,  0,  1,  0}, // Y5  sensor 3 weight
  {0,  1,  1,  0}, // Y6  sensor 4 temperature, humidity
  {1,  1,  1,  0}, // Y7  sensor 4 weight
  {0,  0,  0,  1}, // Y8  sensor 5 temperature, humidity
  {1,  0,  0,  1}, // Y9  sensor 5 weight
  {0,  1,  0,  1}, // Y10 sensor 6 temperature, humidity
  {1,  1,  0,  1}, // Y11 sensor 6 weight
  {0,  0,  1,  1}, // Y12 sensor 7 temperature, humidity
  {1,  0,  1,  1}, // Y13 sensor 7 weight
  {0,  1,  1,  1}, // Y14 sensor 8 temperature, humidity
  {1,  1,  1,  1}  // Y15 sensor 8 weight
};

void YSelect(int Y){
  digitalWrite(SPin[0], STable[Y][0]);
  digitalWrite(SPin[1], STable[Y][1]);
  digitalWrite(SPin[2], STable[Y][2]);
  digitalWrite(SPin[3], STable[Y][3]);
}


//*************************
//**** sensors          ***
//*************************


/** Create struct for data packet */
typedef struct Data{
  bool sensDHT22;                  // DHT22 en-/disabled
  bool sensHX711;                  // HX711 en-/disabled
  int  offset;                     // calibration offset for HX711
  int  factor;                     // calibration factor for HX711
} Data;
Data sensorData[NoSensors];        // valori per i sensori

bool sensDHT22;
bool sensHX711;
int  offset;
int  factor;

float   w;                 // variables for weight (get_units) 
float   h, t;              // variables for humidity and temperature
uint8_t sensorNo;          // sensor number
uint8_t s;                 // variable for sensor


//**********************
//****     WIFI AP  ****
//**********************


AsyncWebServer server(80);

JsonDocument JSONdata;            //create json document

// to reset ESP if no clients connected to WiFi AP
// time until reset (15 minutes)
const unsigned long RESET_TIMEOUT = 15UL * 60UL * 1000UL;
unsigned long noClientSince = 0;


// Variable stays in RTC memory during sleep cycles. Not after a reset.
RTC_DATA_ATTR bool apMode = false; 

void disableWiFi() {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_wifi_deinit();
    btStop();
    D_println("WiFi disabled");
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is MeshBee. You can access webserial interface at http://" + WiFi.softAPIP().toString() + "/webserial");
  });

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);

  /* Attach Message Callback */
  WebSerial.onMessage([&](uint8_t *data, size_t len) {
    Serial.printf("Received %u bytes from WebSerial: ", len);
    Serial.write(data, len);
    Serial.println();
    WebSerial.println("Received Data...");
    String d = "";
    for(size_t i=0; i < len; i++){
      d += char(data[i]);
    }
    WebSerial.println(d);

    if ( d == "list" ) {                                                   // list
      WebSerial.println("list all data"); 
      D_println(d); 
      for ( int i=0 ; i < NoSensors ; i++ ) {
        if (!sensorData[i].sensDHT22){
          WebSerial.print("sensor ");
          WebSerial.print(i);
          WebSerial.println(" DHT22 disabled"); 
        } else {
          WebSerial.print("sensor ");
          WebSerial.print(i);
          WebSerial.println(" DHT22 enabled");
        }
        if (!sensorData[i].sensHX711){
          WebSerial.print("sensor ");
          WebSerial.print(i);
          WebSerial.println(" HX711 disabled"); 
        } else {
          WebSerial.print("sensor "); 
          WebSerial.print(i); 
          WebSerial.print(" HX711 enabled "); 
          WebSerial.print("offset: "); 
          WebSerial.print(sensorData[i].offset); 
          WebSerial.print(", factor: "); 
          WebSerial.println(sensorData[i].factor); 
        }  
      }    
  
    } else if ( d == "ver") {                                              // shows software version
      WebSerial.print ("software version: ");
      WebSerial.println (SoftwareVersion);
    
    } else if ( d == "sleep") {                                            // shows sleep time
      WebSerial.print ("sleep time is: ");
      WebSerial.println (sleep_time);
    
    } else if ( d == "reset") {                                            // reset
      WebSerial.println ("ESP32 restart in 5 seconds");
      delay(5000);
      ESP.restart();
    
    } else if ( d == "example") {                                          // example sensor data
      WebSerial.println ("{\"sensor\":0,\"sensDHT22\": false,\"sensHX711\": false,\"offset\":0,\"factor\":0}");

    } else if ( d == "exsleep") {                                           // example sleep time
      WebSerial.println ("{\"sleep_time\":60}");

    } else if ( d == "erase") {                                            // erase all permanent data
      Serial.println ("erase all permanent stored data");
      nvs_flash_erase(); // erase the NVS partition and...
      nvs_flash_init(); // initialize the NVS partition.

    } else if ( d == "store") {                                            // store all data permanently
      Serial.println ("store all data permanently");
      preferences.begin("SensorData", false);
      for ( int i = 0 ; i < NoSensors ; i++ ){
        String sensDHT22 = "sensDHT22_" + String(i);
        preferences.putBool(sensDHT22.c_str(), sensorData[i].sensDHT22);   // sensor DHT22 enabled ?
        String sensHX711 = "sensHX711_" + String(i);
        preferences.putBool(sensHX711.c_str(), sensorData[i].sensHX711);   // sensor HX711 enabled ?
        String offset = "offset_" + String(i);
        preferences.putInt(offset.c_str(), sensorData[i].offset);          // offset
        String factor = "factor_" + String(i);
        preferences.putInt(factor.c_str(), sensorData[i].factor);          // factor
      }
      preferences.end();

      D_println ("list all stored data");
      for ( int i=0 ; i < NoSensors ; i++ ) {
        if (!sensorData[i].sensDHT22){
          D_printf("DHT22 %d disabled.\n", i);
        } else {
          D_printf("DHT22 %d enabled.\n", i);
        }  
        if (!sensorData[i].sensHX711){
          D_printf("HX711 %d disabled.\n", i);
        } else {
          D_printf("HX711 %d: offset: %d, factor: %d\n", i, sensorData[i].offset, sensorData[i].factor);
        }  
      }    
      WebSerial.println("all data stored permanently");

    } else if ( d == "reload") {                                            // reload all permanetly stored data
      Serial.println ("reload all  permanently stored data");
      preferences.begin("SensorData", false);
      for ( int i = 0 ; i < NoSensors ; i++ ){

        String SensDHT22Str = "sensDHT22_" + String(i);                     // DHT22
        sensDHT22 = preferences.getBool(SensDHT22Str.c_str(), 0); 
        sensorData[i].sensDHT22 = sensDHT22;

        String SensHX711Str = "sensHX711_" + String(i);                     // HX711
        sensHX711 = preferences.getBool(SensHX711Str.c_str(), 0); 
        sensorData[i].sensHX711 = sensHX711;

        String OffsetStr = "offset_" + String(i);                           // offset
        offset = preferences.getInt(OffsetStr.c_str(), 0); 
        sensorData[i].offset = offset;

        String FactorStr = "factor_" + String(i);                           // factor
        factor = preferences.getInt(FactorStr.c_str(), 1);
        sensorData[i].factor = factor;
      }
      String SleepStr = "sleep";                                            // sleep time
      sleep_time = preferences.getInt(SleepStr.c_str(), 1);
      preferences.end();

      D_println ("list all stored data");
      for ( int i=0 ; i < NoSensors ; i++ ) {
        if (!sensorData[i].sensDHT22){
          D_printf("DHT22 %d disabled.\n", i);
        } else {
          D_printf("DHT22 %d enabled.\n", i);
        }  
        if (!sensorData[i].sensHX711){
          D_printf("HX711 %d disabled.\n", i);
        } else {
          D_printf("HX711 %d: offset: %d, factor: %d\n", i, sensorData[i].offset, sensorData[i].factor);
        }  
      }    
      WebSerial.println("all permanent data reloaded");

    } else if (!deserializeJson(JSONdata, d)) {                            // save data for sensors or sleep time
/*
{"sensor":0,"sensDHT22": true,"sensHX711": true,"offset":-234360,"factor":254000}
{"sensor":1,"sensDHT22": true,"sensHX711": true,"offset":-297000,"factor":213462}
{"sensor":2,"sensDHT22": true,"sensHX711": true,"offset":-297000,"factor":260570}
*/
      if (JSONdata["sensor"] != nullptr){
        int noSensor = JSONdata["sensor"];
        if(JSONdata["sensDHT22"]!= nullptr)   sensorData[noSensor].sensDHT22   = JSONdata["sensDHT22"];
        if(JSONdata["sensHX711"] != nullptr)   sensorData[noSensor].sensHX711   = JSONdata["sensHX711"];
        if(JSONdata["offset"]  != nullptr)   sensorData[noSensor].offset      = JSONdata["offset"];
        if(JSONdata["factor"]  != nullptr)   sensorData[noSensor].factor      = JSONdata["factor"];
        D_print ("Sensor: "); D_println(noSensor);
        D_print ("DHT22: ");  D_println(sensorData[noSensor].sensDHT22);
        D_print ("HX711: ");  D_println(sensorData[noSensor].sensHX711);
        D_print ("Offset: "); D_println(sensorData[noSensor].offset);
        D_print ("Factor: "); D_println(sensorData[noSensor].factor);
      }
/*
{"sleep_time":0}
*/
      if (JSONdata["sleep_time"] != nullptr){
        sleep_time = JSONdata["sleep_time"];
        D_print ("sleep time: "); D_print(sleep_time); D_println (" minutes");
      
        preferences.begin("SensorData", false);
        String sleep = "sleep";
        preferences.putInt(sleep.c_str(), sleep_time);   // store sleep time permanently
        preferences.end();
      }
    } else {
      WebSerial.println("unknown command");
    }
  });

  //start Wifi AP, Webserial, OTA-update    
  // Start ElegantOTA
  ElegantOTA.begin(&server);    
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  // Start server
  server.begin();
}


//***********************
//**** wakeup reason ****
//***********************


esp_sleep_wakeup_cause_t wakeup_reason;

/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void wakeupReason() {

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     D_println("Wakeup caused by external signal using RTC_IO"); D_println("Switch on Wifi AP");  apMode = true;  /*  meshSerial.println("Wakeup caused by external signal using RTC_IO"); */ break;
    case ESP_SLEEP_WAKEUP_EXT1:     D_println("Wakeup caused by external signal using RTC_CNTL");        /*  meshSerial.println("Wakeup caused by external signal using RTC_CNTL");  */       break;
    case ESP_SLEEP_WAKEUP_TIMER:    D_println("Wakeup caused by timer");                                 /*  meshSerial.println("Wakeup caused by timer");                           */       break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: D_println("Wakeup caused by touchpad");                              /*  meshSerial.println("Wakeup caused by touchpad");                         */      break;
    case ESP_SLEEP_WAKEUP_ULP:      D_println("Wakeup caused by ULP program");                           /*  meshSerial.println("Wakeup caused by ULP program");                      */      break;
    default:                        D_printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);/* meshSerial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); */ break;
  }
}

/*
Method to print the GPIO that triggered the wakeup
*/
void print_GPIO_wake_up(){
  int GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  Serial.print("GPIO that triggered the wake up: GPIO ");
  Serial.println((log(GPIO_reason))/log(2), 0);
}


//**********************
//**** get data     ****
//**********************


boolean getHX711data(int i){
  bool sensorResult = true;
//  D_print("channel "); D_print(2*i + 1); D_print(" for the weight of sensor "); D_println(i);
  YSelect(2*i+1); // changes the value from 0 to 15 to choose which pin Y to select
  delay(DELAYTIME);
    scale.power_up();
    delay(400);
    scale.set_gain(128);
    scale.set_offset(sensorData[i].offset);  // offset
    scale.set_scale(sensorData[i].factor);   // calibration
    scale.set_median_mode();
    delay(DELAYTIME);
     
  if (scale.is_ready()) {            // check if data pin == LOW - for disabling set data pin = HIGH
    w = 2 * scale.get_units(20);     // taking the weight twice
    D_print("Il peso è "); D_print(w); D_print(" kg"); D_println();  
  } else {
    D_print ("sensor "); D_print (i); D_print (" HX711 not found"); D_println();
    sensorResult = false;
    w = NAN;
  }
  scale.power_down(); 
  return sensorResult;
}


boolean getDHT22data ( int i ){    
  bool sensorResult = true;
//  D_print("channel "); D_print(2*i); D_println(" for temperature and humidity");
  YSelect(2*i); // changes the value from 0 to 15 to choose which pin Y to select
  delay(DELAYTIME);
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)
  delay(DELAYTIME);
  h = dht.readHumidity(true);
  delay(DELAYTIME);
  t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    sensorResult = false;
    D_print ("sensor "); D_print (i); D_print (" DHT22 not found"); D_println();
  } else {
    D_print ("sensor "); D_print (i); D_print(F(" DHT22 Humidity: ")); D_print(h); D_print(F("%  Temperature: ")); D_print(t); D_println(F("°C "));
  }
  return sensorResult;
}


//**********************
//**** JSON message ****
//**********************


void dataPacketCreate(){
  JsonDocument mydata;                         //create json document
  for (int i = 0 ; i < NoSensors ; i++){       // loop for the number of sensors
    D_println();

    if((sensorData[i].sensHX711)){             // if sensor is enabled
      if (!getHX711data(i)){
        M_print ("sensor "); M_print (i); M_print (" error on HX711.");
        delay(400);
      }
    } else {
      w = NAN;
      D_print("sensor "); D_print(i); D_println(" HX711 not enabled. ");
    }
    
    if(sensorData[i].sensDHT22){             // if sensor is enabled
      if (!getDHT22data(i)){
        M_print ("sensor "); M_print (i); M_println (" error on DHT22.");
        delay(400);
      }
    } else {
      t = NAN;
      h = NAN;
      D_print("sensor "); D_print(i); D_println(" DHT22 not enabled.");
    }
    
    if ((sensorData[i].sensHX711) || (sensorData[i].sensDHT22)) {

      String output;

      mydata["sensor"] = i;          // load values into "mydata" variable to convert to JSON
      mydata["humidity"] = h;
      mydata["temperature"] = t;
      mydata["weight"] = w;
      serializeJson(mydata, output); // add mydata to output send to serial

      for (int j = 0; j < NoTransmission; j++){   // number of transmissions via meshtastic
        meshSerial.println(output);  // print data to meshtastic
        delay(1000);
      }
      D_print ("sensor "); D_print (i); D_print(" message: "); D_print(output); D_println(" sent");       // print json data to serial monitor
    }
  }
}


//**********************
//****    setup     ****
//**********************


void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.print("Software version: ");
  Serial.println(SoftwareVersion);
  D_print(NoSensors);
  D_println(" sensors configurable");
 
  pinMode(WAKEUP_GPIO, INPUT_PULLDOWN);                // for wakeup and for reset in AP mode
  pinMode(ALARM_GPIO,  INPUT_PULLDOWN);                // for wakeup and for reset in AP mode
  // Define bitmask for multiple GPIOs
  uint64_t bitmask = BUTTON_PIN_BITMASK(WAKEUP_GPIO) | BUTTON_PIN_BITMASK(ALARM_GPIO);


  // wakeup reason for ESP32
  wakeupReason();

  // get all values out of non volatile storage
  preferences.begin("SensorData", false);

  for ( int i = 0 ; i < NoSensors ; i++ ){
    String SensDHT22Str = "sensDHT22_" + String(i); 
    sensDHT22 = preferences.getBool(SensDHT22Str.c_str(), 0); 
    sensorData[i].sensDHT22 = sensDHT22;

    String SensHX711Str = "sensHX711_" + String(i); 
    sensHX711 = preferences.getBool(SensHX711Str.c_str(), 0); 
    sensorData[i].sensHX711 = sensHX711;

    String OffsetStr = "offset_" + String(i); 
    offset = preferences.getInt(OffsetStr.c_str(), 0); 
    sensorData[i].offset = offset;

    String FactorStr = "factor_" + String(i); 
    factor = preferences.getInt(FactorStr.c_str(), 1);
    sensorData[i].factor = factor;

    if (!sensorData[i].sensDHT22){
      D_printf("DHT22 %d disabled.\n", i);
    } else {
      D_printf("DHT22 %d enabled.\n", i);
    }  
    if (!sensorData[i].sensHX711){
      D_printf("HX711 %d disabled.\n", i);
    } else {
      D_printf("Sensor %d: offset: %d, factor: %d\n", i, sensorData[i].offset, sensorData[i].factor);
    }  
  }
  String SleepStr = "sleep"; 
  sleep_time = preferences.getInt(SleepStr.c_str(), 0); // if there is no sleep time, put default sleep time
  if (sleep_time == 0){
    sleep_time = TIME_TO_SLEEP;
    String sleep = "sleep";
    preferences.putInt(sleep.c_str(), sleep_time);      // store sleep time permanently
  }
  
  preferences.end();

  if (!apMode) {
    disableWiFi();
    setCPUfreq(40);                    // slow down the ESP32 processor if not in APmode


    
    
    
/*    
   If wake-up GPIO == alarm_gpio then send alarm message but still send values. So, you can see which sensor is missing
   print_GPIO_wake_up();

 
 
   https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/
   Achtung EXT1 !!!
   
 
 */
    
    
    
    
    
    
    
    
    
    
    YSelect(1);                        // sensor 1 weight

    // Meshtastic communication
    // Start Serial 2 with the defined RX and TX pins and a baud rate of 115200
    meshSerial.begin(MESH_BAUD, SERIAL_8N1, RXD2, TXD2);
    D_println("Serial 2 'meshSerial' started at 115200 baud rate");


    // Start the DHT22 sensor
    dht.begin();

    // Start the HX711 sensor
    // Initialize library with data output pin, clock input pin and gain factor.
    // Channel selection is made by passing the appropriate gain:
    // - With a gain factor of 64 or 128, channel A is selected
    // - With a gain factor of 32, channel B is selected
    // By omitting the gain factor parameter, the library
    // default "128" (Channel A) is used here.
    // scale.set_gain(128) (or 64) for channel A
    // scale.set_gain(32)          for channel B
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    // Multiplexer gpio as output
    for (int i = 0; i < 4; i++)
    {
      pinMode(SPin[i], OUTPUT);   // Initialize all Spin as OUTPUT
      digitalWrite(SPin[i], LOW); // Set all Spin LOW
    }
 
    // wake up timer
    // First we configure the wake up source. We set our ESP32 to wake up every x seconds
    esp_sleep_enable_timer_wakeup(sleep_time *(60 + OSC_TOLERANCE) * uS_TO_S_FACTOR);
    D_println("sleep time: " + String(sleep_time) + " minutes");
    wake_time = millis() + TIME_TO_BE_AWAKE * 1000;

    dataPacketCreate();
  } else {
    setCPUfreq(240);
    startAP();
    delay(2000);
    attachInterrupt(WAKEUP_GPIO, buttonISR, FALLING);
    D_println("Interrupt on button pressed enabled");
  }
  delay(DELAYTIME);


  // external wake up by bitmask = WAKEUP_GPIO and ALARM_GPIO
  esp_sleep_enable_ext0_wakeup((gpio_num_t)bitmask, 1);  //1 = High, 0 = Low

  delay(DELAYTIME);   //for stabilisation of DHT22 before taking values
}


//**********************
//****    loop     ****
//**********************


void loop() {
  if (pressed) {
    D_println("Button pressed ");
    pressed = false;
    delay(200);
    ESP.restart();
  }
  if (!apMode){
    // going to sleep
    if (millis() >= wake_time){
      wake_time = millis() + TIME_TO_BE_AWAKE * 1000;
      
      //Go to sleep now
      D_println(); D_println("Going to sleep now"); 
      esp_deep_sleep_start();
      D_println("This will never be printed");
    }
  } else {
    WebSerial.loop();
    ElegantOTA.loop();

    uint8_t clients = WiFi.softAPgetStationNum();         // number of clients connectd with WiFi AP
    if (clients > 0) {
      // minimum one client connected
      noClientSince = millis();
    } else {
      // no clients connected -> reset
      if (millis() - noClientSince >= RESET_TIMEOUT) {
        Serial.println("15 minutes no client connected -> reset");
        delay(100);
        ESP.restart();
      }
    }
  }
}
