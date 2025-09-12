#include <Arduino.h>
#include "LoraMesher.h"

//Using LILYGO TTGO T-BEAM v1.1 
#define BOARD_LED   4
#define LED_ON      LOW
#define LED_OFF     HIGH


LoraMesher& radio = LoraMesher::getInstance();

uint32_t dataCounter = 0;
struct dataPacket {
    int type = 0; // 0 for counter, 1 for wan packet
    union {
        uint32_t counter = 0;
        uint8_t data[4];
    } data;
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
    if (data.type == 0)
    {
        Serial.printf("DATA:%d %x\n", data.data.counter, src);
        // Serial.printf("Hello Counte r received nº %d\n", data.data.counter);
    }
    else{
        for(int i = 0; i < 4; i++)
        {
            Serial.println(data.data.data[i]);
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

    //Get the payload to iterate through it
    dataPacket* dPacket = packet->payload;
    size_t payloadLength = packet->getPayloadLength();
    uint16_t src = packet->src;

    for (size_t i = 0; i < payloadLength; i++) {
        //Print the packet
        printPacket(dPacket[i], src);
    }
}

/**
 * @brief Function that process the received packets
 *
 */
void processReceivedPackets(void*) {
    for (;;) {
        /* Wait for the notification of processReceivedPackets and enter blocking */
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        led_Flash(1, 100); //one quick LED flashes to indicate a packet has arrived

        //Iterate through all the packets inside the Received User Packets Queue
        while (radio.getReceivedQueueSize() > 0) {
            Serial.println("ReceivedUserData_TaskHandle notify received");
            Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());

            //Get the first element inside the Received User Packets Queue
            AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();

            //Print the data packet
            printDataPacket(packet);

            //Delete the packet when used. It is very important to call this function to release the memory of the packet.
            radio.deletePacket(packet);
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
    pinMode(BOARD_LED, OUTPUT); //setup pin as output for indicator LED
    led_Flash(2, 125);          //two quick LED flashes to indicate program start
    setupLoraMesher();
}


void loop() {
        for (;;) {

            #ifdef IS_GATEWAY
                if (Serial.available())
                {
                    uint8_t data[7];
                    Serial.readBytes(data, 7);
                    returnPacket->type = 1;
                    // Serial.println("TO SEND: ");
                    for(int i = 0; i < 4; i++)
                    {
                        returnPacket->data.data[i] = data[i];
                        // Serial.println(data[i]);
                    }
                    uint16_t dst = data[5] + 256 * data[4];
                    // Serial.println(dst);
                    Serial.flush();
                    radio.createPacketAndSend(dst, returnPacket, 1);
                }
            #endif

            #ifdef IS_SENSOR_NODE
                Serial.printf("Send packet %d\n", dataCounter);

                helloPacket->data.counter = dataCounter++;
                helloPacket->type = 0;
                
                RouteNode* dst = radio.getClosestGateway(); //Get the closest gateway to send the packet
                uint16_t addr;
                if (dst != nullptr)
                    addr = dst->networkNode.address; //Get the address of the destination node
                else 
                    addr = BROADCAST_ADDR;
                Serial.printf("Sending packet to %X\n", addr);

                //Create packet and send it.
                // radio.createPacketAndSend(BROADCAST_ADDR, helloPacket, 1);
                radio.createPacketAndSend(addr, helloPacket, 1);
            #endif

            //Wait 20 seconds to send the next packet
            vTaskDelay(20000 / portTICK_PERIOD_MS);
        }
}