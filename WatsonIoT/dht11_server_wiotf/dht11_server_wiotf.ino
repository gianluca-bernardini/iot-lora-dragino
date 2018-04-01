/*
  Upload Data to Watson IoT Server
  Support Devices: LG01 
  
  Example sketch showing how to get data from remote LoRa node, 
  Then send the value to IoT Server

  It is designed to work with the other sketch dht11_client. 

  modified 24 11 2016
  by Edwin Chen <support@dragino.com>
  Dragino Technology Co., Limited
  
  modified 27 02 2018
  by Gianluca Bernardini <gianluca.bernardini@it.ibm.com>
  IBM
  
*/

#include <SPI.h>
#include <RH_RF95.h>
#include <Console.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include "YunClient.h"

YunClient yun_client;
RH_RF95 rf95;


#define ORG "ldgwxb"
#define GW_TYPE "dragino-gw"
#define GW_ID "dragino1"
#define METHOD "use-token-auth"
#define TOKEN "<your token>"
#define EVENT_TYPE "status"
#define EVENT_FORMAT "json"
#define DEVICE_TYPE "lora-device"

//For product: LG01. 
#define BAUDRATE 115200

uint16_t crcdata = 0;
uint16_t recCRCData = 0;
float frequency = 868.0;

volatile int interrupt = 0;
int rc = 0;

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char topic[] = "iot-2/type/%s/id/%s/evt/%s/fmt/%s";

char clientId[] = "g:" ORG ":" GW_TYPE ":" GW_ID;


PubSubClient client(server, 1883, NULL, yun_client);


void setup()
{
    Bridge.begin(BAUDRATE);
    Console.begin();
    
    if (!rf95.init())
        Console.println("rf95 init failed");

    // Setup ISM frequency
    rf95.setFrequency(frequency);
    // Setup Power,dBm
    rf95.setTxPower(13);
    
    Console.println("Start Listening ");
}

uint16_t calcByte(uint16_t crc, uint8_t b)
{
    uint32_t i;
    crc = crc ^ (uint32_t)b << 8;
  
    for ( i = 0; i < 8; i++)
    {
      if ((crc & 0x8000) == 0x8000)
        crc = crc << 1 ^ 0x1021;
      else
        crc = crc << 1;
    }
    return crc & 0xffff;
}

uint16_t CRC16(uint8_t *pBuffer, uint32_t length)
{
    uint16_t wCRC16 = 0;
    uint32_t i;
    if (( pBuffer == 0 ) || ( length == 0 ))
    {
        return 0;
    }
    for ( i = 0; i < length; i++)
    {
        wCRC16 = calcByte(wCRC16, pBuffer[i]);
    }
    return wCRC16;
}

uint16_t recdata( unsigned char* recbuf, int Length)
{
    crcdata = CRC16(recbuf, Length - 2); //Get CRC code
    recCRCData = recbuf[Length - 1]; //Calculate CRC Data
    recCRCData = recCRCData << 8; //
    recCRCData |= recbuf[Length - 2];
}

void loop()
{
    if (rf95.waitAvailableTimeout(2000))// Listen Data from LoRa Node
    {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];//receive data buffer
        uint8_t len = sizeof(buf);//data buffer length
        if (rf95.recv(buf, &len))//Check if there is incoming data
        {
            recdata( buf, len);
            if(crcdata == recCRCData) //Check if CRC is correct
            { 
                Console.println("Get Data from LoRa Node");
                uint8_t data[] = "   Server ACK";//Reply  
                data[0] = buf[0];
                data[1] = buf[1];
                data[2] = buf[2];
                
                rf95.send(data, sizeof(data));// Send Reply to LoRa Node
                rf95.waitPacketSent();
                publishDeviceEvent(buf);
                
            }       
         }
         else
         {
              Console.println("recv failed");
          }
     }
}

void publishDeviceEvent(uint8_t buf[]){

  if (!!!client.connected()) {
    Console.print("Reconnecting client to ");
    Console.println(server);
    while (!!!client.connect(clientId, METHOD, TOKEN)) {
      Console.print(".");
      delay(500);
    }
    Console.println("Client connected");
    
  }
  if(!!!client.loop())
  {
    Console.println("PubSubCLient loop failed");
  }

  int newData[4] = {0, 0, 0, 0}; //Store Sensor Data here
  
  for (int i = 0; i < 2; i++)
  {
      newData[i] = buf[i + 3];
  }
  int h = newData[0];
  int t = newData[1];
  char val[10];
  String payload = "{\"d\":{\"temp\":";
  dtostrf(t,1,2, val);
  payload += val;
  payload += ",\"humidity\":";
  dtostrf(h,1,2, val);
  payload += val;
  payload += "}}";

  char deviceId [] = "lora-d001";
  Console.print("Publishing event ");
  Console.println(payload);
  Console.print("for device ");
  Console.println(deviceId);

  // publish the data...  
  char publishTopic[strlen(EVENT_TYPE) + strlen(EVENT_FORMAT) + strlen(DEVICE_TYPE) + strlen(deviceId) +25];
  sprintf(publishTopic, topic, DEVICE_TYPE, deviceId, EVENT_TYPE, EVENT_FORMAT);
  
  Console.print("on topic ");
  Console.println(publishTopic);
  
  if (client.publish(publishTopic,(char*)payload.c_str())) {
    Console.println("Publish ok");
  } else {
    Console.println("Publish failed");
  }
  
}

