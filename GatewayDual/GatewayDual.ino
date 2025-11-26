//working updated gatewaydual.ino

//gatewaydual.ino - Complete Sensor Data LoRaWAN Gateway

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <pgmspace.h>

#define CFG_in866 1
#define BOARD_LED 35  // Heltec WiFi LoRa 32 V3 LED
#define LED_ON LOW
#define LED_OFF HIGH

bool hasJoined = false;
bool hasFoundDR = false;

static const u1_t PROGMEM APPEUI[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

static const u1_t PROGMEM DEVEUI[8] = {0xd1, 0x6e, 0xf9, 0xbb, 0xda, 0xc4, 0x0f, 0xf6 };
void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

static const u1_t PROGMEM APPKEY[16] = {0x41, 0xd1, 0x82, 0xf8, 0x28, 0x09, 0x14, 0x0f, 0xd7, 0xa0, 0x73, 0xe0, 0x75, 0x90, 0x1f, 0x49};
void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// HAL Configuration class for Heltec V3 board
class cHalConfiguration_t: public Arduino_LMIC::HalConfiguration_t
{
  public:
    virtual u1_t queryBusyPin(void) override {return 13;};
    virtual bool queryUsingDcdc(void) override {return true;};
    virtual bool queryUsingDIO2AsRfSwitch(void) override {return true;};
    virtual bool queryUsingDIO3AsTCXOSwitch(void) override {return true;};
};
cHalConfiguration_t myConfig;

// Pin mapping with HAL configuration
const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 12,
    .dio = {14, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
    .pConfig = &myConfig,
};

static osjob_t sendjob;
const unsigned TX_INTERVAL = 25;

void led_Flash(uint16_t flashes, uint16_t delaymS) {
    for (uint16_t i = 0; i < flashes; i++) {
        digitalWrite(BOARD_LED, LED_ON);
        delay(delaymS);
        digitalWrite(BOARD_LED, LED_OFF);
        delay(delaymS);
    }
}

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16) Serial.print('0');
    Serial.print(v, HEX);
}

void decodeSensorData(uint8_t* data, size_t len, const char* context) {
    if (len < 18) {
        Serial.printf("%s: Invalid payload length: %d, expected at least 18\n", context, len);
        return;
    }
    
    auto getInt16 = [](uint8_t* bytes, int idx) {
        int16_t val = (bytes[idx + 1] << 8) | bytes[idx];
        return val >= 32768 ? val - 65536 : val;
    };
    
    float dist = getInt16(data, 0) / 10.0;
    float sound = getInt16(data, 2) / 10.0;
    float co = getInt16(data, 4) / 10.0;
    
    // CHANGED: bytes 6-7 now contain battery percentage instead of smoke status
    int16_t battery_val = getInt16(data, 6);
    float battery = battery_val / 10.0;  // Convert back to percentage
    
    float temp = getInt16(data, 8) / 10.0;
    float hum = getInt16(data, 10) / 10.0;
    float press = getInt16(data, 12) / 10.0;
    float gas = getInt16(data, 14) / 10.0;
    
    uint16_t actual_device_id = (data[16] << 8) | data[17];
    
    Serial.printf("%s: Decoded sensor data:\n", context);
    Serial.printf("  distance_cm: %.1f\n", dist);
    Serial.printf("  sound_db: %.1f\n", sound);
    Serial.printf("  co_ppm: %.1f\n", co);
    Serial.printf("  battery_percent: %.1f%%\n", battery);  // CHANGED: was "smoke: %s"
    Serial.printf("  temperature_c: %.1f\n", temp);
    Serial.printf("  humidity_percent: %.1f\n", hum);
    Serial.printf("  pressure_hpa: %.1f\n", press);
    Serial.printf("  gas_kohm: %.1f\n", gas);
    Serial.printf("  actual_device_id: 0x%04X\n", actual_device_id);
}

void onEvent(ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            led_Flash(2, 100);
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            led_Flash(3, 100);
            {
                u4_t netid = 0;
                devaddr_t devaddr = 0;
                u1_t nwkKey[16];
                u1_t artKey[16];
                LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
                Serial.print("netid: ");
                Serial.println(netid, DEC);
                Serial.print("devaddr: ");
                Serial.println(devaddr, HEX);
                Serial.print("AppSKey: ");
                for (size_t i = 0; i < sizeof(artKey); ++i) {
                    if (i != 0) Serial.print("-");
                    printHex2(artKey[i]);
                }
                Serial.println("");
                Serial.print("NwkSKey: ");
                for (size_t i = 0; i < sizeof(nwkKey); ++i) {
                    if (i != 0) Serial.print("-");
                    printHex2(nwkKey[i]);
                }
                Serial.println();
            }
            LMIC_setLinkCheckMode(0);
            hasJoined = true;
            Serial.println(F("*** LoRaWAN joined! Ready to send sensor data ***"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            led_Flash(1, 50);
            if (LMIC.txrxFlags & TXRX_ACK)
                Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
                Serial.print(F("Received "));
                Serial.print(LMIC.dataLen);
                Serial.println(F(" bytes of payload"));
                Serial.print("RETURN:");
                for (int i = 0; i < LMIC.dataLen; i++) {
                    Serial.print((LMIC.frame[LMIC.dataBeg + i]));
                    Serial.print(" ");
                }
                Serial.println();
            }
            // Send another packet if haven't found optimal data rate yet (only for initial connection)
            if (!hasFoundDR && !hasJoined)
            {
                uint8_t data[] = "hello";
                do_send(data, sizeof(data) - 1);
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned)ev);
            break;
    }
}

void do_send(uint8_t myData[], size_t size) {
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending (transmission in progress)"));
        return;
    }
    
    // For sensor data (18+ bytes), show detailed info
    if (size >= 18) {
        Serial.printf("=== SENDING SENSOR DATA ===\n");
        Serial.printf("Sending %d bytes to LoRaWAN network\n", size);
        Serial.print("Raw payload: ");
        for (size_t i = 0; i < size; i++) {
            Serial.printf("%d ", myData[i]);
        }
        Serial.println();
        Serial.print("Hex payload: ");
        for (size_t i = 0; i < size; i++) {
            Serial.printf("%02X ", myData[i]);
        }
        Serial.println();
        decodeSensorData(myData, size, "Transmitting");
        Serial.println("=============================");
    } else {
        // For initialization packets
        Serial.printf("Sending %d bytes (init/hello packet)\n", size);
    }
    
    // Queue the data for transmission
    int status = LMIC_setTxData2(1, myData, size, 0); // Use port 1
    
    if (status == -1) {
        Serial.println("ERROR: Data rate adjustment needed - Data NOT sent");
        hasFoundDR = false;
    } else if (status == 0) {
        Serial.println("SUCCESS: Data queued for transmission");
        if (size >= 18) {
            hasFoundDR = true; // Mark as found for sensor data
            Serial.println(">>> Sensor data successfully queued to LoRaWAN!");
        }
    } else {
        Serial.printf("LMIC_setTxData2 returned status: %d\n", status);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.setTimeout(10000);
    
    Serial.println(F("=== LoRaWAN Sensor Data Gateway ==="));
    Serial.println(F("Initializing Heltec WiFi LoRa 32 V3..."));
    
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_OFF);
    led_Flash(2, 250);
    
    Serial.println(F("Heltec board detected"));
    
    #ifdef VCC_ENABLE
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif
    
    // Initialize LMIC
    os_init();
    LMIC_reset();
    
    // Set SF12 for better range/reliability during join
    LMIC_setDrTxpow(DR_SF7, 14);
    
    Serial.println(F("Configured for IN866 band"));
    
    // Send initial packet to trigger OTAA join
    Serial.println(F("Triggering OTAA join process..."));
    uint8_t initData[] = "Hello";
    do_send(initData, sizeof(initData) - 1);
    
    // Serial.println(F("Gateway ready! Send sensor data as: num1 num2 num3 ... (18+ numbers)"));
    // Serial.println(F("Example: 123 45 67 89 12 34 56 78 90 11 22 33 44 55 66 77 88 99"));
    Serial.flush();
}

void loop() {
    static uint8_t buffer[51];
    static String asciiBuffer = "";
    
    // Process incoming serial data
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            // Parse the received string
            if (asciiBuffer.length() > 0) {
                Serial.printf("\n>>> Received: %s\n", asciiBuffer.c_str());
                
                // Parse ASCII numbers separated by spaces
                int numbers[51];
                int count = 0;
                
                // Create a copy for strtok (since it modifies the string)
                char inputCopy[asciiBuffer.length() + 1];
                asciiBuffer.toCharArray(inputCopy, asciiBuffer.length() + 1);
                
                char* token = strtok(inputCopy, " ");
                while (token != NULL && count < 51) {
                    numbers[count] = atoi(token);
                    if (numbers[count] >= 0 && numbers[count] <= 255) {
                        buffer[count] = (uint8_t)numbers[count];
                        count++;
                    }
                    token = strtok(NULL, " ");
                }
                
                Serial.printf("Parsed %d bytes from input\n", count);
                
                // Validate and send sensor data
                if (count >= 18) {
                    if (hasJoined) {
                        Serial.println("LoRaWAN is joined - sending sensor data...");
                        do_send(buffer, count);
                    } else {
                        Serial.println("WARNING: Not joined to LoRaWAN network yet!");
                        Serial.println("Waiting for EV_JOINED event before sending sensor data...");
                    }
                } else {
                    Serial.printf("ERROR: Need at least 18 bytes for sensor data, got %d\n", count);
                    Serial.println("Please send data in format: num1 num2 num3 ... (18+ numbers 0-255)");
                }
                
                asciiBuffer = "";
            }
        } else if (c != '\r') {  // Ignore carriage return
            asciiBuffer += c;
        }
    }
    
    // Run LMIC background tasks
    os_runloop_once();
}