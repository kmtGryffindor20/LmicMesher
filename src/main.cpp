#include <Arduino.h>
#include "LoraMesher.h"

//Using LILYGO TTGO T-BEAM v1.1 
#define BOARD_LED   4
#define LED_ON      LOW
#define LED_OFF     HIGH


// Sensor variables
float lastDistanceCM = 0.0;
float lastSoundDB = 0.0;
float lastCOPpm = 0.0;
String lastSmokeStatus = "NA";
float lastBME680Temp = 0.0;
float lastBME680Humidity = 0.0;
float lastBME680Pressure = 0.0;
float lastBME680Gas = 0.0;


LoraMesher& radio = LoraMesher::getInstance();

uint32_t dataCounter = 0;
struct dataPacket {
    int type = 0; // 0 for sensor, 1 for wan packet
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
            uint8_t node_id_high; // Added for node ID
            uint8_t node_id_low;  // Added for node ID
        } sensor; // 16 bytes total
        uint8_t data[4];
    } u;
};

dataPacket* helloPacket = new dataPacket;

dataPacket* returnPacket = new dataPacket;

//Led flash
void led_Flash(uint16_t flashes, uint16_t delaymS) {
    uint16_t index;
    for (index = 1; index <= flashes; index++) {
        digitalWrite(BOARD_LED, LED_ON);
        delay(delaymS);
        digitalWrite(BOARD_LED, LED_OFF);
        delay(delaymS);
    }
}

/**
 * @brief Print the counter of the packet
 *
 * @param data
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
        Serial.flush(); // Ensure immediate output
    } else {
        for (int i = 0; i < 4; i++) {
            Serial.println(data.u.data[i]);
        }
    }
}

/**
 * @brief Iterate through the payload of the packet and print the counter of the packet
 *
 * @param packet
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

/**
 * @brief Function that process the received packets
 *
 */
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
 * @brief Create a Receive Messages Task and add it to the LoRaMesher
 *
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
    }

    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
}


/**
 * @brief Initialize LoRaMesher
 *
 */
void setupLoraMesher() {
    // Example on how to change the module. See LoraMesherConfig to see all the configurable parameters.
    LoraMesher::LoraMesherConfig config;
    #ifdef HELTEC
        config.module = LoraMesher::LoraModules::SX1262_MOD;
    #endif

    #ifndef HELTEC
        config.module = LoraMesher::LoraModules::SX1276_MOD;
    #endif

    config.loraCs = CS;
    config.loraRst = RST;
    config.loraIrq = IRQ;
    config.loraIo1 = IO1;
    config.sf = 12;
    config.power = 20;

    config.syncWord = 0x45;
    #ifdef HELTEC
        SPI.begin(9, 11, 10, CS); //Initialize SPI with the correct pins
        config.spi = &SPI;
    #endif

    //Init the loramesher with a processReceivedPackets function
    radio.begin(config);

    //Create the receive task and add it to the LoRaMesher
    createReceiveMessages();

    //Start LoRaMesher
    radio.start();

    #ifdef IS_GATEWAY
        radio.addGatewayRole();
    #endif

    Serial.println("Lora initialized");
}


void setup() {
    Serial.begin(115200);

    Serial.println("initBoard");
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LOW);

    setupLoraMesher();
}


void loop() {
        for (;;) {

            #ifdef IS_GATEWAY
                // if (Serial.available())
                // {
                //     uint8_t data[7];
                //     Serial.readBytes(data, 7);
                //     returnPacket->type = 1;
                //     // Serial.println("TO SEND: ");
                //     for(int i = 0; i < 4; i++)
                //     {
                //         returnPacket->data.data[i] = data[i];
                //         // Serial.println(data[i]);
                //     }
                //     uint16_t dst = data[5] + 256 * data[4];
                //     // Serial.println(dst);
                //     Serial.flush();
                //     radio.createPacketAndSend(dst, returnPacket, 1);
                // }
            #endif

            #ifndef IS_GATEWAY
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
            Serial.print(" ppm | Smoke: ");
            Serial.println(lastSmokeStatus);
            Serial.print("BME680/688 - Temp: ");
            Serial.print(lastBME680Temp, 1);
            Serial.print(" °C | Humidity: ");
            Serial.print(lastBME680Humidity, 1);
            Serial.print(" % | Pressure: ");
            Serial.print(lastBME680Pressure, 1);
            Serial.print(" hPa | Gas: ");
            Serial.print(lastBME680Gas, 1);
            Serial.println(" KΩ");
            Serial.println("========================");

            // Prepare and send packet
            uint16_t localAddr = radio.getLocalAddress(); // Declared here to fix undefined error
            helloPacket->type = 0;
            helloPacket->u.sensor.distance = (int16_t)(lastDistanceCM * 10 + 0.5);
            helloPacket->u.sensor.sound = (int16_t)(lastSoundDB * 10 + 0.5);
            helloPacket->u.sensor.co = (int16_t)(lastCOPpm * 10 + 0.5);
            helloPacket->u.sensor.smoke = (lastSmokeStatus == "Detected") ? 1 : 0;
            helloPacket->u.sensor.temp = (int16_t)(lastBME680Temp * 10 + 0.5);
            helloPacket->u.sensor.hum = (int16_t)(lastBME680Humidity * 10 + 0.5);
            helloPacket->u.sensor.press = (int16_t)(lastBME680Pressure * 10 + 0.5);
            helloPacket->u.sensor.gas = (int16_t)(lastBME680Gas * 10 + 0.5);
            helloPacket->u.sensor.node_id_high = (localAddr >> 8) & 0xFF;
            helloPacket->u.sensor.node_id_low = localAddr & 0xFF;

            RouteNode* dst = radio.getClosestGateway();
            uint16_t addr;
            if (dst != nullptr) {
                addr = dst->networkNode.address;
            } else {
                addr = BROADCAST_ADDR;
            }
            Serial.printf("Sending own packet to %X (local addr: 0x%X)\n", addr, localAddr);

            radio.createPacketAndSend(addr, helloPacket, 1);
            //Wait 20 seconds to send the next packet
            vTaskDelay(20000 / portTICK_PERIOD_MS);
            #endif

        }
}