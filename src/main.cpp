// // static const u1_t PROGMEM DEVEUI[8]={0xc5, 0xa6, 0x71, 0x6d, 0xe9, 0x4f, 0xd6, 0xaf};
// // static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// // static const u1_t PROGMEM APPKEY[16] = {0x51,0x72,0x9f,0xa0,0x65,0x2e,0x18,0xec,0x4a,0xc9,0x20,0x18,0xcf,0x1f,0xfc,0xc2};
// #include <Arduino.h>
// // --- LoRaMesher Includes ---
// #include "LoraMesher.h"

// // --- Conditional LoRaWAN Includes for Gateway ---
// #ifdef IS_GATEWAY
// #include <lmic.h>
// #include <hal/hal.h>
// #include <SPI.h>
// #include <pgmspace.h>
// #define CFG_in866 1 // Or your region
// #endif

// // --- BOARD/LED Configuration (TTGO T-BEAM v1.1) ---
// #define BOARD_LED 4 
// #define LED_ON LOW
// #define LED_OFF HIGH
// #define RADIO_POWER_PIN 14 

// // --- LoRaMesher Global Variables (Shared Pins) ---
// #define CS 18
// #define RST 23
// #define IRQ 26
// #define IO1 33 
// #define IO2 32 

// // 🚨 CRITICAL: LMIC Pin Map defined only for Gateway
// #ifdef IS_GATEWAY
// const lmic_pinmap lmic_pins = {
//     .nss = CS,
//     .rxtx = LMIC_UNUSED_PIN,
//     .rst = RST,
//     .dio = {IRQ, IO1, IO2}, 
//     .pConfig = nullptr,
// };
// #endif

// // --- Shared Global Variables ---
// LoraMesher& radio = LoraMesher::getInstance();
// uint32_t dataCounter = 0;

// // Maximum size for "helloX" string (e.g., "hello4294967295\0" is ~18 bytes)
// #define MAX_HELLO_SIZE 20 

// // Modified data structure to send "helloX" string
// struct dataPacket {
//     int type = 0; // 0 for hello string
//     char helloString[MAX_HELLO_SIZE] = {0}; // The actual payload: "helloX"
// };

// // --- Gateway Mode Global Variables (LoRaWAN Forwarding) ---
// #ifdef IS_GATEWAY
// uint8_t wan_payload_buffer[51]; // Max size of LoRaWAN payload
// size_t wan_payload_length = 0;
// TaskHandle_t receiveLoRaMessage_Handle = NULL; 

// bool hasJoined = false;
// // LoRaWAN Keys (Replace with your actual keys)
// static const u1_t PROGMEM DEVEUI[8]={0xc5, 0xa6, 0x71, 0x6d, 0xe9, 0x4f, 0xd6, 0xaf}; 
// static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// static const u1_t PROGMEM APPKEY[16] = {0x51,0x72,0x9f,0xa0,0x65,0x2e,0x18,0xec,0x4a,0xc9,0x20,0x18,0xcf,0x1f,0xfc,0xc2};
// void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
// void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
// void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// // Forward declarations for Gateway functions
// void do_send(uint8_t myData[], size_t size);
// void setupLoraMesherRx();
// void stopLoraMesherRx();
// void startLoraMesherRx();
// #endif

// // --- General Functions ---
// void led_Flash(uint16_t flashes, uint16_t delaymS) {
//     for (uint16_t i = 0; i < flashes; i++) {
//         digitalWrite(BOARD_LED, LED_ON);
//         delay(delaymS);
//         digitalWrite(BOARD_LED, LED_OFF);
//         delay(delaymS);
//     }
// }

// // --- LoRaMesher Core Functions ---
// void setupLoraMesher() {
//     LoraMesher::LoraMesherConfig config;
//     config.module = LoraMesher::LoraModules::SX1276_MOD;
//     config.loraCs = CS;
//     config.loraRst = RST;
//     config.loraIrq = IRQ;
//     config.loraIo1 = IO1;
//     config.syncWord = 0x45;
    
//     // Explicitly initialize SPI bus
//     SPI.begin(5, 19, 27, 18); 
    
//     radio.begin(config);

//     #ifndef IS_GATEWAY
//         radio.setReceiveAppDataTaskHandle(xTaskGetCurrentTaskHandle());
//         radio.start();
//         Serial.println("LoraMesher initialized and running as Sender Node.");
//     #endif
// }

// // --- Gateway Mode Functions (IS_GATEWAY defined) ---
// #ifdef IS_GATEWAY

// /**
//  * @brief Processes the received LoRaMesher packets, extracts data, and prepares for LoRaWAN.
//  * This task is set to run on Core 1 for dedicated mesh listening.
//  */
// void processReceivedPackets(void*) {
//     for (;;) {
//         // Core 1 is continuously waiting here for mesh data
//         ulTaskNotifyTake(pdPASS, portMAX_DELAY); // Wait for packet notification
//         led_Flash(1, 100);

//         while (radio.getReceivedQueueSize() > 0) {
//             Serial.println("\n--- LoRaMesher Packet Received on Core 1 ---");
//             AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();
            
//             if (packet->payloadSize == 1 && packet->payload[0].type == 0) {
//                 char* helloStr = packet->payload[0].helloString;
//                 uint16_t src = packet->src;

//                 Serial.printf("Received Mesh Data: %s from Node: %X\n", helloStr, src);

//                 // --- DATA EXTRACTION AND PREPARATION ---
//                 // Find the counter value (the number after "hello")
//                 uint32_t counterValue = 0;
//                 char* numStart = strstr(helloStr, "hello");
//                 if (numStart != nullptr) {
//                     // Move pointer past "hello"
//                     numStart += 5; 
//                     counterValue = strtoul(numStart, NULL, 10);
//                 }
                
//                 Serial.printf("Extracted Counter Value: %lu\n", counterValue);

//                 // Prepare the binary payload for LoRaWAN (4 bytes counter + 2 bytes source)
//                 wan_payload_buffer[0] = (uint8_t)(counterValue >> 0);
//                 wan_payload_buffer[1] = (uint8_t)(counterValue >> 8);
//                 wan_payload_buffer[2] = (uint8_t)(counterValue >> 16);
//                 wan_payload_buffer[3] = (uint8_t)(counterValue >> 24);
//                 wan_payload_buffer[4] = (uint8_t)(src >> 0);
//                 wan_payload_buffer[5] = (uint8_t)(src >> 8);
//                 wan_payload_length = 6; 

//                 // --- CRITICAL MODE SWITCHING ---
//                 stopLoraMesherRx(); // **1. Stop Mesh to free the radio**
                
//                 if (hasJoined) {
//                     Serial.println("Switching to LoRaWAN mode to send data...");
//                     // **2. Start LoRaWAN TX on Core 0/LMIC**
//                     do_send(wan_payload_buffer, wan_payload_length); 
//                 } else {
//                     Serial.println("LoRaWAN not joined, dropping packet. Restarting LoRaMesher.");
//                     startLoraMesherRx(); // If not joined, go back to listening
//                 }
//             } else {
//                 Serial.printf("Received unknown packet type/size from %X, size %d\n", packet->src, packet->payloadSize);
//             }

//             radio.deletePacket(packet);
//         }
//     }
// }

// /**
//  * @brief Initializes and starts the LoRaMesher stack in gateway/receive mode
//  */
// void setupLoraMesherRx() {
//     Serial.println("--- Starting LoRaMesher RX Mode on Core 1 ---");
    
//     int res = xTaskCreatePinnedToCore(
//         processReceivedPackets,
//         "Receive App Task",
//         4096,
//         (void*) 1,
//         2,
//         &receiveLoRaMessage_Handle,
//         1); // Pin to Core 1
//     if (res != pdPASS) {
//         Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
//     }
//     radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);

//     radio.addGatewayRole();
//     radio.start(); // Start mesh listening initially
// }

// /**
//  * @brief Stops LoRaMesher to free the radio for LoRaWAN
//  */
// void stopLoraMesherRx() {
//     Serial.println("--- Stopping LoRaMesher for LoRaWAN TX ---");
//     radio.standby(); // Stop all tasks and radio activity
// }

// /**
//  * @brief Restarts LoRaMesher to return to receive mode
//  * This is called ONLY after LoRaWAN TX is guaranteed complete via EV_TXCOMPLETE.
//  */
// void startLoraMesherRx() {
//     Serial.println("--- Restarting LoRaMesher RX Mode ---");
//     radio.start();
//     radio.addGatewayRole(); 
// }

// // --- LoRaWAN Functions (LMIC on Core 0) ---
// void do_send(uint8_t myData[], size_t size) {
//     if (LMIC.opmode & OP_TXRXPEND) {
//         Serial.println(F("OP_TXRXPEND, not sending (transmission in progress)"));
//         startLoraMesherRx(); // Go back to mesh listening if LMIC is busy
//         return;
//     }
    
//     Serial.printf("=== STARTING LoRaWAN TX ===\n");
//     LMIC_setTxData2(1, myData, size, 0); // Queue the data
//     Serial.println("Data queued for LoRaWAN transmission. LMIC will handle TX on Core 0.");
// }

// void onEvent(ev_t ev) {
//     switch (ev) {
//         case EV_JOINED:
//             Serial.println(F("EV_JOINED"));
//             hasJoined = true;
//             break;
//         case EV_TXCOMPLETE:
//             Serial.println(F("EV_TXCOMPLETE (TX done)"));
//             led_Flash(1, 50);
//             // 🚨 CRITICAL: LoRaWAN TX is complete. Return to LoRaMesher RX mode.
//             startLoraMesherRx();
//             break;
//         case EV_JOINING:
//             Serial.println(F("EV_JOINING"));
//             break;
//         case EV_JOIN_FAILED:
//             Serial.println(F("EV_JOIN_FAILED. Restarting LoRaMesher."));
//             startLoraMesherRx();
//             break;
//         default:
//             break;
//     }
// }
// #endif // IS_GATEWAY

// // --- Arduino Setup ---
// void setup() {
//     Serial.begin(115200);
//     while (!Serial) delay(100);
    
//     pinMode(BOARD_LED, OUTPUT);
//     digitalWrite(BOARD_LED, LED_OFF);
//     led_Flash(2, 125); 

//     setupLoraMesher();
    
//     #ifdef IS_GATEWAY
//         Serial.println("--- Gateway Dual Mode Start ---");
//         radio.standby();
        
//         // LoRaWAN Setup (LMIC)
//         pinMode(RADIO_POWER_PIN, OUTPUT);
//         digitalWrite(RADIO_POWER_PIN, HIGH);
//         delay(1000); 

//         digitalWrite(RST, LOW);
//         delay(200);
//         digitalWrite(RST, HIGH);
//         delay(200);

//         taskDISABLE_INTERRUPTS();
//         os_init(); 
//         taskENABLE_INTERRUPTS();
        
//         LMIC_reset();
//         LMIC_setLinkCheckMode(0);
//         LMIC_setDrTxpow(DR_SF7, 14); 
        
//         LMIC_startJoining();

//         setupLoraMesherRx(); 
        
//     #else
//         Serial.println("--- Sender Node Mode Start ---");
//     #endif
// }

// // --- Arduino Loop ---
// void loop() {
//     #ifdef IS_GATEWAY
//         // Core 0 handles the LMIC scheduler
//         os_runloop_once();
//         vTaskDelay(10 / portTICK_PERIOD_MS); 
//     #else
//         // SENDER NODE FUNCTIONALITY: Runs on default core
//         dataPacket* helloPacket = new dataPacket;

//         // Create the "helloX" string
//         snprintf(helloPacket->helloString, MAX_HELLO_SIZE, "hello%lu", dataCounter++);
//         helloPacket->type = 0;
        
//         RouteNode* dst = radio.getClosestGateway(); 
//         uint16_t addr = (dst != nullptr) ? dst->networkNode.address : BROADCAST_ADDR;

//         Serial.printf("Send packet %s. Sending to %X\n", helloPacket->helloString, addr);

//         radio.createPacketAndSend(addr, helloPacket, 1);

//         // Wait 20 seconds to send the next packet
//         vTaskDelay(20000 / portTICK_PERIOD_MS);
//         delete helloPacket;
//     #endif
// }













// -------------------------------------------------

// #include <Arduino.h>
// #include <LoraMesher.h>
// #ifndef IS_GATEWAY
// #include <Wire.h>
// #include <Adafruit_Sensor.h>
// #include <Adafruit_BME680.h>
// #include <XPowersLib.h>
// #include "driver/adc.h"
// #include <SparkFun_APDS9960.h>

// // Heltec Sensor pins
// #ifdef HELTEC
//     #define IR_PIN 6
//     #define SENSOR_PIN 19  // For MAX9814 sound sensor
//     #define MQ2_PIN 7     // For MQ2 gas sensor
//     #define I2C_SDA 41     // I2C SDA for BME688
//     #define I2C_SCL 42     // I2C SCL for BME688
    
//     // Heltec Battery Settings
//     #define ADC_CTRL_PIN 37
//     #define VEXT_CTRL_PIN 36
//     const adc1_channel_t VBAT_CHANNEL = ADC1_CHANNEL_0;
//     const float DIVIDER_RATIO = (100.0f + 390.0f) / 100.0f;
//     const float ADC_REF = 3.5f;
//     const float HELTEC_BATTERY_MIN_VOLTAGE = 2.80f;
//     const float HELTEC_BATTERY_MAX_VOLTAGE = 4.22f;
// #else
//     // Lilygo Sensor pins
//     #define IR_PIN 39
//     #define SENSOR_PIN 15  // For MAX9814 sound sensor
//     #define MQ2_PIN 35     // For MQ2 gas sensor
//     #define I2C_SDA 21     // I2C SDA for BME688
//     #define I2C_SCL 22     // I2C SCL for BME688
    
//     // LilyGO Battery Settings
//     XPowersAXP2101 PMU;
//     const float LILYGO_BATTERY_MAX_VOLTAGE = 4.20;
//     const float LILYGO_BATTERY_MIN_VOLTAGE = 3.05;
//     bool pmuInitialized = false;
// #endif

// // BME680/688 setup
// Adafruit_BME680 bme(&Wire);
// #define BME680_I2C_ADDR_1 0x76
// #define BME680_I2C_ADDR_2 0x77

// // APDS-9960 setup
// SparkFun_APDS9960 apds = SparkFun_APDS9960();

// // Sensor variables
// float lastDistanceCM = 0.0;
// float lastSoundDB = 0.0;
// float lastCOPpm = 0.0;
// float lastBatteryPercent = 0.0;  // Replaces smokestatus
// float lastBME680Temp = 0.0;
// float lastBME680Humidity = 0.0;
// float lastBME680Pressure = 0.0;
// float lastBME680Gas = 0.0;
// uint16_t lastRed = 0;
// uint16_t lastGreen = 0;
// uint16_t lastBlue = 0;

// // APDS Selective Transmission Variables
// uint16_t lastRedSent = 0;
// uint16_t lastGreenSent = 0;
// uint16_t lastBlueSent = 0;
// unsigned long lastAPDSSendTime = 0;
// const unsigned long APDS_CHECK_INTERVAL = 300000; // 5 minutes in milliseconds

// const int sampleWindow = 100;
// const float MQ2_THRESHOLD = 1.0;
// unsigned long lastSensorRead = 0;
// const int sensorInterval = 5000; // Read every 5 seconds
// int bmeReadingCount = 0;
// int bmeFailedAttempts = 0;
// bool bmeInitialized = false;

// #ifdef HELTEC
// void enableVBATdivider() {
//     pinMode(ADC_CTRL_PIN, OUTPUT);
//     digitalWrite(ADC_CTRL_PIN, HIGH);
//     pinMode(VEXT_CTRL_PIN, OUTPUT);
//     digitalWrite(VEXT_CTRL_PIN, LOW);
//     delay(20);
// }

// void disableVBATdivider() {
//     pinMode(ADC_CTRL_PIN, INPUT);
//     pinMode(VEXT_CTRL_PIN, INPUT);
// }

// void configADC() {
//     adc1_config_width(ADC_WIDTH_BIT_12);
//     adc1_config_channel_atten(VBAT_CHANNEL, ADC_ATTEN_DB_11);
// }

// float readVBAT() {
//     int raw = adc1_get_raw(VBAT_CHANNEL);
//     return ((raw / 4095.0f) * ADC_REF) * DIVIDER_RATIO;
// }

// float heltecVoltageToPercent(float v) {
//     if (v <= HELTEC_BATTERY_MIN_VOLTAGE) return 0;
//     if (v >= HELTEC_BATTERY_MAX_VOLTAGE) return 100;
//     return ((v - HELTEC_BATTERY_MIN_VOLTAGE) / (HELTEC_BATTERY_MAX_VOLTAGE - HELTEC_BATTERY_MIN_VOLTAGE)) * 100;
// }

// float getBatteryPercentage() {
//     enableVBATdivider();
//     float vbat = readVBAT();
//     float percent = heltecVoltageToPercent(vbat);
//     disableVBATdivider();
    
//     // If voltage too low but percent is 0, likely on USB
//     if (vbat < 3.0 && percent == 0.0) {
//         return 100.0;  // USB powered
//     }
//     return percent;
// }
// #else
// // Battery Functions for LilyGO
// float lilygoVoltageToPercent(float voltage) {
//     if (voltage >= LILYGO_BATTERY_MAX_VOLTAGE) return 100.0;
//     if (voltage <= LILYGO_BATTERY_MIN_VOLTAGE) return 0.0;
//     return ((voltage - LILYGO_BATTERY_MIN_VOLTAGE) / (LILYGO_BATTERY_MAX_VOLTAGE - LILYGO_BATTERY_MIN_VOLTAGE)) * 100.0;
// }

// float getBatteryPercentage() {
//     if (!pmuInitialized) return 0.0;
    
//     if (!PMU.isBatteryConnect()) {
//         return 100.0;  // USB powered
//     }
    
//     float battVoltage = PMU.getBattVoltage() / 1000.0;
//     return lilygoVoltageToPercent(battVoltage);
// }
// #endif

// #endif

// //Using LILYGO TTGO T-BEAM v1.1 
// #define BOARD_LED 4
// #define LED_ON LOW
// #define LED_OFF HIGH

// LoraMesher& radio = LoraMesher::getInstance();

// struct dataPacket {
//     int type = 0; // 0 for sensor, 1 for wan packet
//     union {
//         struct {
//             int16_t distance;
//             int16_t sound;
//             int16_t co;
//             int16_t smoke;  // Now contains battery percentage (int16_t for compatibility)
//             int16_t temp;
//             int16_t hum;
//             int16_t press;
//             int16_t gas;
//             int16_t red;
//             int16_t green;
//             int16_t blue;
//             uint8_t node_id_high;
//             uint8_t node_id_low;
//         } sensor; // Expanded for RGB
//         uint8_t data[4];
//     } u;
// };

// dataPacket* helloPacket = new dataPacket;
// dataPacket* returnPacket = new dataPacket;

// //Led flash
// void led_Flash(uint16_t flashes, uint16_t delaymS) {
//     uint16_t index;
//     for (index = 1; index <= flashes; index++) {
//         digitalWrite(BOARD_LED, LOW);
//         delay(delaymS);
//         digitalWrite(BOARD_LED, LOW);
//         delay(delaymS);
//     }
// }

// /**
//  * @brief Print the counter of the packet
//  */
// void printPacket(dataPacket data, uint16_t src) {
//     if (data.type == 0) {
//         Serial.print("DATA:");
//         uint8_t* bytes = (uint8_t*)&data.u.sensor;
//         for (size_t i = 0; i < sizeof(data.u.sensor); i++) {
//             Serial.print(bytes[i]);
//             if (i < sizeof(data.u.sensor) - 1) Serial.print(" ");
//         }
//         Serial.printf(" %x\n", src);
//         Serial.flush(); // Ensure immediate output
//     } else {
//         for (int i = 0; i < 4; i++) {
//             Serial.println(data.u.data[i]);
//         }
//     }
// }

// /**
//  * @brief Iterate through the payload of the packet
//  */
// void printDataPacket(AppPacket<dataPacket>* packet) {
//     Serial.printf("Packet arrived from %X with size %d\n", packet->src, packet->payloadSize);
//     dataPacket* dPacket = packet->payload;
//     size_t payloadLength = packet->getPayloadLength();
//     uint16_t src = packet->src;
//     for (size_t i = 0; i < payloadLength; i++) {
//         printPacket(dPacket[i], src);
//     }
// }

// void processReceivedPackets(void*) {
//     for (;;) {
//         ulTaskNotifyTake(pdPASS, portMAX_DELAY);
//         led_Flash(1, 100);
//         while (radio.getReceivedQueueSize() > 0) {
//             Serial.println("ReceivedUserData_TaskHandle notify received");
//             Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());
//             AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();
//             if (packet != nullptr) {
//                 printDataPacket(packet);
//                 radio.deletePacket(packet);
//             }
//         }
//     }
// }

// TaskHandle_t receiveLoRaMessage_Handle = NULL;

// /**
//  * @brief Create a Receive Messages Task
//  */
// void createReceiveMessages() {
//     int res = xTaskCreate(
//         processReceivedPackets,
//         "Receive App Task",
//         4096,
//         (void*)1,
//         2,
//         &receiveLoRaMessage_Handle);
//     if (res != pdPASS) {
//         Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
//     }
//     radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
// }

// void setupLoraMesher() {
//     LoraMesher::LoraMesherConfig config;
//     #ifdef HELTEC
//         config.module = LoraMesher::LoraModules::SX1262_MOD;
//         SPI.begin(9, 11, 10, CS);
//         config.spi = &SPI;
//     #else
//         config.module = LoraMesher::LoraModules::SX1276_MOD;
//     #endif
//     config.loraCs = CS;
//     config.loraRst = RST;
//     config.loraIrq = IRQ;
//     config.loraIo1 = IO1;
//     config.syncWord = 0x45;
    
//     radio.begin(config);
//     createReceiveMessages();
//     radio.start();
    
//     #ifdef IS_GATEWAY
//         radio.addGatewayRole();
//     #endif
//     Serial.println("Lora initialized");
// }

// void setup() {
//     Serial.begin(115200);
//     delay(1000); // Ensure Serial is ready
//     Serial.println("initBoard");
//     pinMode(BOARD_LED, OUTPUT);
//     led_Flash(2, 125);
    
//     setupLoraMesher();
    
//     // Print local node address for identification
//     uint16_t localAddr = radio.getLocalAddress();
//     Serial.printf("Local node address: 0x%X\n", localAddr);
//     Serial.flush();

// #ifndef IS_GATEWAY
//     // Initialize sensor pins
//     pinMode(IR_PIN, INPUT);
//     pinMode(SENSOR_PIN, INPUT);
//     pinMode(MQ2_PIN, INPUT);
    
//     // Initialize I2C
//     Serial.println("Initializing I2C bus...");
//     Wire.begin(I2C_SDA, I2C_SCL);
//     Wire.setClock(100000); // Standard 100kHz I2C clock
    
//     // Initialize Battery Management
//     #ifdef HELTEC
//         Serial.println("Initializing Heltec Battery Management...");
//         configADC();
//         Serial.println("Heltec battery management initialized");
//     #else
//         Serial.println("Initializing LilyGO AXP2101...");
//         if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
//             PMU.enableBattVoltageMeasure();
//             PMU.enableBattDetection();
//             pmuInitialized = true;
//             Serial.println("AXP2101 initialized successfully");
//         } else {
//             Serial.println("Failed to initialize AXP2101");
//         }
//     #endif
    
//     // Initialize BME680/688
//     Serial.println("Initializing BME680/688...");
//     for (uint8_t addr : {BME680_I2C_ADDR_1, BME680_I2C_ADDR_2}) {
//         Serial.print("Trying BME680/688 at address 0x");
//         Serial.println(addr, HEX);
//         if (bme.begin(addr)) {
//             bme.setTemperatureOversampling(BME680_OS_8X);
//             bme.setHumidityOversampling(BME680_OS_2X);
//             bme.setPressureOversampling(BME680_OS_4X);
//             bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
//             bme.setGasHeater(320, 150);
//             Serial.println("BME680/688 initialized successfully at address 0x" + String(addr, HEX));
//             bmeInitialized = true;
//             break;
//         } else {
//             Serial.println("Failed to initialize BME680/688 at address 0x" + String(addr, HEX));
//         }
//     }
//     if (!bmeInitialized) {
//         Serial.println("BME680/688 unavailable. Check wiring, I2C address, or sensor.");
//         lastBME680Temp = lastBME680Humidity = lastBME680Pressure = lastBME680Gas = -999.0;
//     }
    
//     // Initialize APDS-9960
//     Serial.println("Initializing APDS-9960...");
//     if (apds.init()) {
//         Serial.println("APDS-9960 initialization complete");
//     } else {
//         Serial.println("Something went wrong during APDS-9960 init!");
//     }
//     if (apds.enableLightSensor(false)) {
//         Serial.println("Light sensor is now running");
//     } else {
//         Serial.println("Something went wrong during light sensor init!");
//     }
    
//     // Allow sensors to stabilize
//     delay(2000);
//     Serial.println("--------------------------------");
// #endif
// }

// /**
//  * @brief Check if APDS readings are all zero
//  */
// bool isAPDSReadingZero(uint16_t red, uint16_t green, uint16_t blue) {
//     return (red == 0 && green == 0 && blue == 0);
// }

// /**
//  * @brief Check if APDS readings have changed significantly
//  */
// bool hasAPDSReadingChanged(uint16_t red, uint16_t green, uint16_t blue) {
//     return (red != lastRedSent || green != lastGreenSent || blue != lastBlueSent);
// }

// void loop() {
//     for (;;) {
//         #ifdef IS_GATEWAY
//             if (Serial.available() >= 7) {
//                 uint8_t data[7];
//                 Serial.readBytes(data, 7);
//                 returnPacket->type = 1;
//                 for (int i = 0; i < 4; i++) {
//                     returnPacket->u.data[i] = data[i];
//                 }
//                 uint16_t dst = (data[4] << 8) | data[5];
//                 Serial.flush();
//                 radio.createPacketAndSend(dst, returnPacket, 1);
//                 Serial.printf("Gateway sent response to 0x%X\n", dst);
//             }
//         #endif
        
//         #ifndef IS_GATEWAY
//             bmeReadingCount++;
            
//             // 1. IR Distance
//             int irSamples = 10;
//             float irSum = 0;
//             for (int i = 0; i < irSamples; i++) {
//                 irSum += analogRead(IR_PIN);
//                 delay(10);
//             }
//             float irVoltage = (irSum / irSamples) * (3.3 / 4095.0);
//             lastDistanceCM = (irVoltage > 0.2) ? 26.0 * pow(irVoltage, -1.10) : 150.0;
            
//             // 2. MAX9814 Sound
//             unsigned long startMillis = millis();
//             unsigned int signalMax = 0;
//             unsigned int signalMin = 4095;
//             unsigned int sample;
//             while (millis() - startMillis < sampleWindow) {
//                 sample = analogRead(SENSOR_PIN);
//                 if (sample > signalMax) signalMax = sample;
//                 if (sample < signalMin) signalMin = sample;
//             }
//             int peakToPeak = signalMax - signalMin;
//             float voltagePP = (peakToPeak * 3.3) / 4095.0;
//             float rms = voltagePP / 2.0 / sqrt(2);
//             lastSoundDB = 20.0 * log10(rms / 0.00631);
//             if (lastSoundDB < 0 || isnan(lastSoundDB)) lastSoundDB = 0;
            
//             // 3. MQ2 Gas Sensor
//             int mq2Value = analogRead(MQ2_PIN);
//             float mq2Voltage = mq2Value * (3.3 / 4095.0);
//             lastCOPpm = mq2Voltage * 20;
            
//             // 4. Battery Percentage (replaces smoke status)
//             lastBatteryPercent = getBatteryPercentage();
            
//             // 5. BME680/688 Environmental Sensor
//             bool bmeSuccess = false;
//             if (bmeInitialized) {
//                 bmeSuccess = bme.performReading();
//             }
//             if (bmeSuccess) {
//                 lastBME680Temp = bme.temperature;
//                 lastBME680Humidity = bme.humidity;
//                 lastBME680Pressure = bme.pressure / 100.0;
//                 lastBME680Gas = bme.gas_resistance > 0 ? bme.gas_resistance / 4000.0 : 0.0;
//                 bmeFailedAttempts = 0;
//             } else {
//                 bmeFailedAttempts++;
//                 Serial.println("Failed to perform BME680/688 reading. Attempt: " + String(bmeFailedAttempts));
//                 if (bmeFailedAttempts > 5) {
//                     lastBME680Temp = lastBME680Humidity = lastBME680Pressure = lastBME680Gas = -999.0;
//                 }
//             }
            
//             // 6. APDS-9960 RGB
//             uint16_t red_light, green_light, blue_light;
//             if (!apds.readRedLight(red_light) ||
//                 !apds.readGreenLight(green_light) ||
//                 !apds.readBlueLight(blue_light)) {
//                 Serial.println("Error reading RGB values");
//                 lastRed = lastGreen = lastBlue = 0;
//             } else {
//                 lastRed = red_light;
//                 lastGreen = green_light;
//                 lastBlue = blue_light;
//             }
            
//             // Print to Serial for debug
//             Serial.println("\n=== Sensor Readings ===");
//             Serial.print("IR Distance: ");
//             Serial.print(lastDistanceCM, 1);
//             Serial.println(" cm");
//             Serial.print("Sound Level: ");
//             Serial.print(lastSoundDB, 1);
//             Serial.println(" dB");
//             Serial.print("MQ2 - CO PPM: ");
//             Serial.print(lastCOPpm, 1);
//             Serial.println(" ppm");
//             Serial.print("Battery: ");
//             Serial.print(lastBatteryPercent, 1);
//             Serial.println(" %");
//             Serial.print("BME680/688 - Temp: ");
//             Serial.print(lastBME680Temp, 1);
//             Serial.print(" Â°C | Humidity: ");
//             Serial.print(lastBME680Humidity, 1);
//             Serial.print(" % | Pressure: ");
//             Serial.print(lastBME680Pressure, 1);
//             Serial.print(" hPa | Gas: ");
//             Serial.print(lastBME680Gas, 1);
//             Serial.println(" KÎ©");
//             Serial.print("RGB - Red: ");
//             Serial.print(lastRed);
//             Serial.print(" Green: ");
//             Serial.print(lastGreen);
//             Serial.print(" Blue: ");
//             Serial.println(lastBlue);
            
//             // Determine if APDS data should be sent
//             bool sendAPDSData = false;
//             unsigned long currentTime = millis();
            
//             if (currentTime - lastAPDSSendTime >= APDS_CHECK_INTERVAL) {
//                 // 5 minutes have passed, check if reading is non-zero
//                 if (!isAPDSReadingZero(lastRed, lastGreen, lastBlue)) {
//                     sendAPDSData = true;
//                     Serial.println("APDS: Non-zero reading detected, will send data");
//                 } else {
//                     Serial.println("APDS: Zero reading detected, skipping transmission");
//                 }
//                 lastAPDSSendTime = currentTime;
//             } else if (hasAPDSReadingChanged(lastRed, lastGreen, lastBlue)) {
//                 // Reading changed before 5 minute mark, send if non-zero
//                 if (!isAPDSReadingZero(lastRed, lastGreen, lastBlue)) {
//                     sendAPDSData = true;
//                     Serial.println("APDS: Reading changed to non-zero, sending data");
//                     lastAPDSSendTime = currentTime;
//                 } else {
//                     Serial.println("APDS: Reading changed to zero, skipping transmission");
//                 }
//             }
            
//             Serial.println("========================");
            
//             // Only update last sent APDS values if we're sending
//             if (sendAPDSData) {
//                 lastRedSent = lastRed;
//                 lastGreenSent = lastGreen;
//                 lastBlueSent = lastBlue;
//             }
            
//             // Prepare and send packet
//             uint16_t localAddr = radio.getLocalAddress();
//             helloPacket->type = 0;
//             helloPacket->u.sensor.distance = (int16_t)(lastDistanceCM * 10 + 0.5);
//             helloPacket->u.sensor.sound = (int16_t)(lastSoundDB * 10 + 0.5);
//             helloPacket->u.sensor.co = (int16_t)(lastCOPpm * 10 + 0.5);
//             helloPacket->u.sensor.smoke = (int16_t)(lastBatteryPercent * 10 + 0.5);  // Battery % in smoke field
//             helloPacket->u.sensor.temp = (int16_t)(lastBME680Temp * 10 + 0.5);
//             helloPacket->u.sensor.hum = (int16_t)(lastBME680Humidity * 10 + 0.5);
//             helloPacket->u.sensor.press = (int16_t)(lastBME680Pressure * 10 + 0.5);
//             helloPacket->u.sensor.gas = (int16_t)(lastBME680Gas * 10 + 0.5);
            
//             // Only include non-zero APDS data if we determined it should be sent
//             if (sendAPDSData) {
//                 helloPacket->u.sensor.red = (int16_t) min(32767, (int)lastRed);
//                 helloPacket->u.sensor.green = (int16_t) min(32767, (int)lastGreen);
//                 helloPacket->u.sensor.blue = (int16_t) min(32767, (int)lastBlue);
//             } else {
//                 // Send zero values for RGB to indicate no light detected
//                 helloPacket->u.sensor.red = 0;
//                 helloPacket->u.sensor.green = 0;
//                 helloPacket->u.sensor.blue = 0;
//             }
            
//             helloPacket->u.sensor.node_id_high = (localAddr >> 8) & 0xFF;
//             helloPacket->u.sensor.node_id_low = localAddr & 0xFF;
            
//             RouteNode* dst = radio.getClosestGateway();
//             uint16_t addr;
//             if (dst != nullptr) {
//                 addr = dst->networkNode.address;
//             } else {
//                 addr = BROADCAST_ADDR;
//             }
//             Serial.printf("Sending own packet to %X (local addr: 0x%X)\n", addr, localAddr);
//             radio.createPacketAndSend(addr, helloPacket, 1);
//         #endif   
//         vTaskDelay(20000/ portTICK_PERIOD_MS);
//     }
// }


// -------------------------------------


#include <Arduino.h>
#include <LoraMesher.h>

#ifndef IS_GATEWAY
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <XPowersLib.h>
#include "driver/adc.h"
#include <SparkFun_APDS9960.h>

// Heltec Sensor pins
#ifdef HELTEC
    #define IR_PIN 6
    #define SENSOR_PIN 19  // For MAX9814 sound sensor
    #define MQ2_PIN 7     // For MQ2 gas sensor
    #define I2C_SDA 41     // I2C SDA for BME688
    #define I2C_SCL 42     // I2C SCL for BME688
    
    // Heltec Battery Settings
    #define ADC_CTRL_PIN 37
    #define VEXT_CTRL_PIN 36
    const adc1_channel_t VBAT_CHANNEL = ADC1_CHANNEL_0;
    const float DIVIDER_RATIO = (100.0f + 390.0f) / 100.0f;
    const float ADC_REF = 3.5f;
    const float HELTEC_BATTERY_MIN_VOLTAGE = 2.80f;
    const float HELTEC_BATTERY_MAX_VOLTAGE = 4.22f;
#else
    // Lilygo Sensor pins
    #define IR_PIN 39
    #define SENSOR_PIN 15  // For MAX9814 sound sensor
    #define MQ2_PIN 35     // For MQ2 gas sensor
    #define I2C_SDA 21     // I2C SDA for BME688
    #define I2C_SCL 22     // I2C SCL for BME688
    
    // LilyGO Battery Settings
    XPowersAXP2101 PMU;
    const float LILYGO_BATTERY_MAX_VOLTAGE = 4.20;
    const float LILYGO_BATTERY_MIN_VOLTAGE = 3.05;
    bool pmuInitialized = false;
#endif

// BME680/688 setup
Adafruit_BME680 bme(&Wire);
#define BME680_I2C_ADDR_1 0x76
#define BME680_I2C_ADDR_2 0x77

// APDS-9960 setup
SparkFun_APDS9960 apds = SparkFun_APDS9960();

// APDS Selective Transmission Variables (declared here, used in loop)
uint16_t lastRedSent = 0;
uint16_t lastGreenSent = 0;
uint16_t lastBlueSent = 0;
unsigned long lastAPDSSendTime = 0;
unsigned long lastAPDSCheckTime = 0;
bool apdsIsZero = true;
const unsigned long APDS_CHECK_INTERVAL = 300000;
const unsigned long APDS_RECHECK_INTERVAL = 5000;

// Sensor variables
float lastDistanceCM = 0.0;
float lastSoundDB = 0.0;
float lastCOPpm = 0.0;
float lastBatteryPercent = 0.0;
float lastBME680Temp = 0.0;
float lastBME680Humidity = 0.0;
float lastBME680Pressure = 0.0;
float lastBME680Gas = 0.0;
uint16_t lastRed = 0;
uint16_t lastGreen = 0;
uint16_t lastBlue = 0;

// APDS Selective Transmission Variables
// uint16_t lastRedSent = 0;
// uint16_t lastGreenSent = 0;
// uint16_t lastBlueSent = 0;
// unsigned long lastAPDSSendTime = 0;
// const unsigned long APDS_CHECK_INTERVAL = 300000; // 5 minutes in milliseconds

const int sampleWindow = 100;
const float MQ2_THRESHOLD = 1.0;
unsigned long lastSensorRead = 0;
const int sensorInterval = 5000;
int bmeReadingCount = 0;
int bmeFailedAttempts = 0;
bool bmeInitialized = false;

#ifdef HELTEC
void enableVBATdivider() {
    pinMode(ADC_CTRL_PIN, OUTPUT);
    digitalWrite(ADC_CTRL_PIN, HIGH);
    pinMode(VEXT_CTRL_PIN, OUTPUT);
    digitalWrite(VEXT_CTRL_PIN, LOW);
    delay(20);
}

void disableVBATdivider() {
    pinMode(ADC_CTRL_PIN, INPUT);
    pinMode(VEXT_CTRL_PIN, INPUT);
}

void configADC() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(VBAT_CHANNEL, ADC_ATTEN_DB_11);
}

float readVBAT() {
    int raw = adc1_get_raw(VBAT_CHANNEL);
    return ((raw / 4095.0f) * ADC_REF) * DIVIDER_RATIO;
}

float heltecVoltageToPercent(float v) {
    if (v <= HELTEC_BATTERY_MIN_VOLTAGE) return 0;
    if (v >= HELTEC_BATTERY_MAX_VOLTAGE) return 100;
    return ((v - HELTEC_BATTERY_MIN_VOLTAGE) / (HELTEC_BATTERY_MAX_VOLTAGE - HELTEC_BATTERY_MIN_VOLTAGE)) * 100;
}

float getBatteryPercentage() {
    enableVBATdivider();
    float vbat = readVBAT();
    float percent = heltecVoltageToPercent(vbat);
    disableVBATdivider();
    
    if (vbat < 3.0 && percent == 0.0) {
        return 100.0;
    }
    return percent;
}
#else
// Battery Functions for LilyGO
float lilygoVoltageToPercent(float voltage) {
    if (voltage >= LILYGO_BATTERY_MAX_VOLTAGE) return 100.0;
    if (voltage <= LILYGO_BATTERY_MIN_VOLTAGE) return 0.0;
    return ((voltage - LILYGO_BATTERY_MIN_VOLTAGE) / (LILYGO_BATTERY_MAX_VOLTAGE - LILYGO_BATTERY_MIN_VOLTAGE)) * 100.0;
}

float getBatteryPercentage() {
    if (!pmuInitialized) return 0.0;
    
    if (!PMU.isBatteryConnect()) {
        return 100.0;
    }
    
    float battVoltage = PMU.getBattVoltage() / 1000.0;
    return lilygoVoltageToPercent(battVoltage);
}
#endif

#endif

// Using LILYGO TTGO T-BEAM v1.1
#define BOARD_LED 4
#define LED_ON LOW
#define LED_OFF HIGH

LoraMesher& radio = LoraMesher::getInstance();

struct dataPacket {
    int type = 0;
    union {
        struct {
            int16_t distance;
            int16_t sound;
            int16_t co;
            int16_t smoke;
            int16_t temp;
            int16_t hum;
            int16_t press;
            int16_t gas;
            int16_t red;
            int16_t green;
            int16_t blue;
            uint8_t node_id_high;
            uint8_t node_id_low;
        } sensor;
        uint8_t data[4];
    } u;
};

dataPacket* helloPacket = new dataPacket;
dataPacket* returnPacket = new dataPacket;

// Led flash
void led_Flash(uint16_t flashes, uint16_t delaymS) {
    uint16_t index;
    for (index = 1; index <= flashes; index++) {
        digitalWrite(BOARD_LED, LOW);
        delay(delaymS);
        digitalWrite(BOARD_LED, LOW);
        delay(delaymS);
    }
}

/**
 * @brief Print the counter of the packet
 */
void printPacket(dataPacket data, uint16_t src) {
    if (data.type == 0) {
        Serial.print("DATA:");
        uint8_t* bytes = (uint8_t*)&data.u.sensor;
        for (size_t i = 0; i < sizeof(data.u.sensor); i++) {
            Serial.print(bytes[i]);
            if (i < sizeof(data.u.sensor) - 1) Serial.print(" ");
        }
        Serial.printf(" %x\n", src);
        Serial.flush();
    } else {
        for (int i = 0; i < 4; i++) {
            Serial.println(data.u.data[i]);
        }
    }
}

/**
 * @brief Iterate through the payload of the packet
 */
void printDataPacket(AppPacket<dataPacket>* packet) {
    Serial.printf("Packet arrived from %X with size %d\n", packet->src, packet->payloadSize);
    dataPacket* dPacket = packet->payload;
    size_t payloadLength = packet->getPayloadLength();
    uint16_t src = packet->src;
    for (size_t i = 0; i < payloadLength; i++) {
        printPacket(dPacket[i], src);
    }
}

void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        led_Flash(1, 100);
        while (radio.getReceivedQueueSize() > 0) {
            Serial.println("ReceivedUserData_TaskHandle notify received");
            Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());
            AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();
            if (packet != nullptr) {
                printDataPacket(packet);
                radio.deletePacket(packet);
            }
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create a Receive Messages Task
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*)1,
        2,
        &receiveLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
    }
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
}

void setupLoraMesher() {
    LoraMesher::LoraMesherConfig config;
    #ifdef HELTEC
        config.module = LoraMesher::LoraModules::SX1262_MOD;
        SPI.begin(9, 11, 10, CS);
        config.spi = &SPI;
    #else
        config.module = LoraMesher::LoraModules::SX1276_MOD;
    #endif
    config.loraCs = CS;
    config.loraRst = RST;
    config.loraIrq = IRQ;
    config.loraIo1 = IO1;
    config.syncWord = 0x45;
    
    radio.begin(config);
    createReceiveMessages();
    radio.start();
    
    #ifdef IS_GATEWAY
        radio.addGatewayRole();
    #endif
    Serial.println("Lora initialized");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("initBoard");
    pinMode(BOARD_LED, OUTPUT);
    led_Flash(2, 125);
    
    setupLoraMesher();
    
    uint16_t localAddr = radio.getLocalAddress();
    Serial.printf("Local node address: 0x%X\n", localAddr);
    Serial.flush();

#ifndef IS_GATEWAY
    pinMode(IR_PIN, INPUT);
    pinMode(SENSOR_PIN, INPUT);
    pinMode(MQ2_PIN, INPUT);
    
    Serial.println("Initializing I2C bus...");
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    
    #ifdef HELTEC
        Serial.println("Initializing Heltec Battery Management...");
        configADC();
        Serial.println("Heltec battery management initialized");
    #else
        Serial.println("Initializing LilyGO AXP2101...");
        if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
            PMU.enableBattVoltageMeasure();
            PMU.enableBattDetection();
            pmuInitialized = true;
            Serial.println("AXP2101 initialized successfully");
        } else {
            Serial.println("Failed to initialize AXP2101");
        }
    #endif
    
    Serial.println("Initializing BME680/688...");
    for (uint8_t addr : {BME680_I2C_ADDR_1, BME680_I2C_ADDR_2}) {
        Serial.print("Trying BME680/688 at address 0x");
        Serial.println(addr, HEX);
        if (bme.begin(addr)) {
            bme.setTemperatureOversampling(BME680_OS_8X);
            bme.setHumidityOversampling(BME680_OS_2X);
            bme.setPressureOversampling(BME680_OS_4X);
            bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
            bme.setGasHeater(320, 150);
            Serial.println("BME680/688 initialized successfully at address 0x" + String(addr, HEX));
            bmeInitialized = true;
            break;
        } else {
            Serial.println("Failed to initialize BME680/688 at address 0x" + String(addr, HEX));
        }
    }
    if (!bmeInitialized) {
        Serial.println("BME680/688 unavailable. Check wiring, I2C address, or sensor.");
        lastBME680Temp = lastBME680Humidity = lastBME680Pressure = lastBME680Gas = -999.0;
    }
    
    Serial.println("Initializing APDS-9960...");
    if (apds.init()) {
        Serial.println("APDS-9960 initialization complete");
    } else {
        Serial.println("Something went wrong during APDS-9960 init!");
    }
    if (apds.enableLightSensor(false)) {
        Serial.println("Light sensor is now running");
    } else {
        Serial.println("Something went wrong during light sensor init!");
    }
    
    delay(2000);
    Serial.println("--------------------------------");
#endif
}

/**
 * @brief Check if APDS readings are all zero
 */
bool isAPDSReadingZero(uint16_t red, uint16_t green, uint16_t blue) {
    return (red == 0 && green == 0 && blue == 0);
}

/**
 * @brief Check if APDS readings have changed significantly
 */
bool hasAPDSReadingChanged(uint16_t red, uint16_t green, uint16_t blue) {
    return (red != lastRedSent || green != lastGreenSent || blue != lastBlueSent);
}

void loop() {
    for (;;) {
        #ifdef IS_GATEWAY
            if (Serial.available() >= 7) {
                uint8_t data[7];
                Serial.readBytes(data, 7);
                returnPacket->type = 1;
                for (int i = 0; i < 4; i++) {
                    returnPacket->u.data[i] = data[i];
                }
                uint16_t dst = (data[4] << 8) | data[5];
                Serial.flush();
                radio.createPacketAndSend(dst, returnPacket, 1);
                Serial.printf("Gateway sent response to 0x%X\n", dst);
            }
        #endif
        
        #ifndef IS_GATEWAY
            bmeReadingCount++;
            
            // 1. IR Distance
            int irSamples = 10;
            float irSum = 0;
            for (int i = 0; i < irSamples; i++) {
                irSum += analogRead(IR_PIN);
                delay(10);
            }
            float irVoltage = (irSum / irSamples) * (3.3 / 4095.0);
            lastDistanceCM = (irVoltage > 0.2) ? 26.0 * pow(irVoltage, -1.10) : 150.0;
            
            // 2. MAX9814 Sound
            unsigned long startMillis = millis();
            unsigned int signalMax = 0;
            unsigned int signalMin = 4095;
            unsigned int sample;
            while (millis() - startMillis < sampleWindow) {
                sample = analogRead(SENSOR_PIN);
                if (sample > signalMax) signalMax = sample;
                if (sample < signalMin) signalMin = sample;
            }
            int peakToPeak = signalMax - signalMin;
            float voltagePP = (peakToPeak * 3.3) / 4095.0;
            float rms = voltagePP / 2.0 / sqrt(2);
            lastSoundDB = 20.0 * log10(rms / 0.00631);
            if (lastSoundDB < 0 || isnan(lastSoundDB)) lastSoundDB = 0;
            
            // 3. MQ2 Gas Sensor
            int mq2Value = analogRead(MQ2_PIN);
            float mq2Voltage = mq2Value * (3.3 / 4095.0);
            lastCOPpm = mq2Voltage * 20;
            
            // 4. Battery Percentage
            lastBatteryPercent = getBatteryPercentage();
            
            // 5. BME680/688 Environmental Sensor
            bool bmeSuccess = false;
            if (bmeInitialized) {
                bmeSuccess = bme.performReading();
            }
            if (bmeSuccess) {
                lastBME680Temp = bme.temperature;
                lastBME680Humidity = bme.humidity;
                lastBME680Pressure = bme.pressure / 100.0;
                lastBME680Gas = bme.gas_resistance > 0 ? bme.gas_resistance / 4000.0 : 0.0;
                bmeFailedAttempts = 0;
            } else {
                bmeFailedAttempts++;
                Serial.println("Failed to perform BME680/688 reading. Attempt: " + String(bmeFailedAttempts));
                if (bmeFailedAttempts > 5) {
                    lastBME680Temp = lastBME680Humidity = lastBME680Pressure = lastBME680Gas = -999.0;
                }
            }
            
            // 6. APDS-9960 RGB
            uint16_t red_light, green_light, blue_light;
            if (!apds.readRedLight(red_light) ||
                !apds.readGreenLight(green_light) ||
                !apds.readBlueLight(blue_light)) {
                Serial.println("Error reading RGB values");
                lastRed = lastGreen = lastBlue = 0;
            } else {
                lastRed = red_light;
                lastGreen = green_light;
                lastBlue = blue_light;
            }
            
            // Print to Serial for debug
            Serial.println("\n=== Sensor Readings ===");
            Serial.print("IR Distance: ");
            Serial.print(lastDistanceCM, 1);
            Serial.println(" cm");
            Serial.print("Sound Level: ");
            Serial.print(lastSoundDB, 1);
            Serial.println(" dB");
            Serial.print("MQ2 - CO PPM: ");
            Serial.print(lastCOPpm, 1);
            Serial.println(" ppm");
            Serial.print("Battery: ");
            Serial.print(lastBatteryPercent, 1);
            Serial.println(" %");
            Serial.print("BME680/688 - Temp: ");
            Serial.print(lastBME680Temp, 1);
            Serial.print(" Â°C | Humidity: ");
            Serial.print(lastBME680Humidity, 1);
            Serial.print(" % | Pressure: ");
            Serial.print(lastBME680Pressure, 1);
            Serial.print(" hPa | Gas: ");
            Serial.print(lastBME680Gas, 1);
            Serial.println(" KÎ©");
            Serial.print("RGB - Red: ");
            Serial.print(lastRed);
            Serial.print(" Green: ");
            Serial.print(lastGreen);
            Serial.print(" Blue: ");
            Serial.println(lastBlue);
            
            // Check APDS status - if zero, skip all transmission
            bool currentAPDSIsZero = isAPDSReadingZero(lastRed, lastGreen, lastBlue);
            
            if (currentAPDSIsZero) {
                // APDS is zero - skip everything, no data transmission
                if (!apdsIsZero) {
                    // Just changed to zero
                    Serial.println("APDS: Became ZERO - stopping all transmissions!");
                    apdsIsZero = true;
                    lastAPDSCheckTime = millis();
                } else {
                    // Still zero - just monitoring
                    if (millis() - lastAPDSCheckTime >= APDS_RECHECK_INTERVAL) {
                        Serial.println("APDS: Still ZERO - waiting for light...");
                        lastAPDSCheckTime = millis();
                    }
                }
                Serial.println("========================");
                // Skip to next iteration without sending anything
                vTaskDelay(30000 / portTICK_PERIOD_MS);
                continue;
            } else {
                // APDS is non-zero - send all data
                if (apdsIsZero) {
                    // Just changed to non-zero
                    Serial.println("APDS: Became NON-ZERO - resuming transmissions!");
                    apdsIsZero = false;
                }
                
                Serial.println("========================");
                
                // Prepare and send packet with all sensor data + RGB
                uint16_t localAddr = radio.getLocalAddress();
                helloPacket->type = 0;
                helloPacket->u.sensor.distance = (int16_t)(lastDistanceCM * 10 + 0.5);
                helloPacket->u.sensor.sound = (int16_t)(lastSoundDB * 10 + 0.5);
                helloPacket->u.sensor.co = (int16_t)(lastCOPpm * 10 + 0.5);
                helloPacket->u.sensor.smoke = (int16_t)(lastBatteryPercent * 10 + 0.5);
                helloPacket->u.sensor.temp = (int16_t)(lastBME680Temp * 10 + 0.5);
                helloPacket->u.sensor.hum = (int16_t)(lastBME680Humidity * 10 + 0.5);
                helloPacket->u.sensor.press = (int16_t)(lastBME680Pressure * 10 + 0.5);
                helloPacket->u.sensor.gas = (int16_t)(lastBME680Gas * 10 + 0.5);
                helloPacket->u.sensor.red = (int16_t) min(32767, (int)lastRed);
                helloPacket->u.sensor.green = (int16_t) min(32767, (int)lastGreen);
                helloPacket->u.sensor.blue = (int16_t) min(32767, (int)lastBlue);
                helloPacket->u.sensor.node_id_high = (localAddr >> 8) & 0xFF;
                helloPacket->u.sensor.node_id_low = localAddr & 0xFF;
                
                RouteNode* dst = radio.getClosestGateway();
                uint16_t addr;
                if (dst != nullptr) {
                    addr = dst->networkNode.address;
                } else {
                    addr = BROADCAST_ADDR;
                }
                Serial.printf("Sending complete packet to %X (local addr: 0x%X)\n", addr, localAddr);
                radio.createPacketAndSend(addr, helloPacket, 1);
                
                lastRedSent = lastRed;
                lastGreenSent = lastGreen;
                lastBlueSent = lastBlue;
            }
        #endif   
        vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
}