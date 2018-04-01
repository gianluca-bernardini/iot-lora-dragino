/*
  Upload Data to Watson IoT Server
  Support Devices: LoRa Shield + Arduino 
  
  Example sketch showing how to read Temperature and Humidity from DHT11 sensor,  
  Then send the value to LoRa Server, the LoRa Server will send the value to the 
  IoT server

  It is designed to work with the other sketch dht11_server_wiotf. 

  modified 24 11 2016
  by Edwin Chen <support@dragino.com>
  Dragino Technology Co., Limited
  
  modified 27 02 2018
  by Gianluca Bernardini <gianluca.bernardini@it.ibm.com>
  IBM
*/
#include "DHT.h"
#include <SPI.h>
#include <RH_RF95.h>
#include <String.h>

RH_RF95 rf95;

#define DHTPIN 8      // digital pin for DHT sensor
#define DHTTYPE DHT11 // DHT 11 type of the DHT sensor


byte bGlobalErr;
float frequency = 868.0;

DHT dht(DHTPIN, DHTTYPE);

void setup()
{
    Serial.begin(9600);
    if (!rf95.init())
        Serial.println("init failed");
    // Setup ISM frequency
    rf95.setFrequency(frequency);
    // Setup Power,dBm
    rf95.setTxPower(13);

    dht.begin();
    
    Serial.println("Humidity and temperature\n\n"); 
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

uint16_t CRC16(uint8_t *pBuffer,uint32_t length)
{
    uint16_t wCRC16=0;
    uint32_t i;
    if (( pBuffer==0 )||( length==0 ))
    {
      return 0;
    }
    for ( i = 0; i < length; i++)
    { 
      wCRC16 = calcByte(wCRC16, pBuffer[i]);
    }
    return wCRC16;
}

void loop()
{
    // Read Humidity
    float dht_h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float dht_t = dht.readTemperature();
    // Compute heat index in Celsius (isFahreheit = false)
    float dht_hic = dht.computeHeatIndex(dht_t, dht_h, false);

    Serial.print("DHT - temp:" );
    Serial.print(dht_t);
    Serial.print(" - humidity:" );
    Serial.print(dht_h);
    Serial.print(" - heat:" );
    Serial.println(dht_hic);

    char data[50] = {0} ;
    // Use data[0], data[1],data[2] as Node ID
    data[0] = 1 ;
    data[1] = 1 ;
    data[2] = 1 ;
    data[3] = dht_h;//Get Humidity
    data[4] = dht_t;//Get Temperature
 
    int dataLength = strlen(data);//CRC length for LoRa Data
    //Serial.println(dataLength);
     
    uint16_t crcData = CRC16((unsigned char*)data,dataLength);//get CRC DATA
    //Serial.println(crcData);
       
    unsigned char sendBuf[50]={0};
    int i;
    for(i = 0;i < 8;i++)
    {
        sendBuf[i] = data[i] ;
    }
    
    sendBuf[dataLength] = (unsigned char)crcData; // Add CRC to LoRa Data
    //Serial.println( sendBuf[dataLength] );
    
    sendBuf[dataLength+1] = (unsigned char)(crcData>>8); // Add CRC to LoRa Data
    //Serial.println( sendBuf[dataLength+1] );

    rf95.send(sendBuf, strlen((char*)sendBuf));//Send LoRa Data
    //Serial.print(strlen((char*)sendBuf));
     
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];//Reply data array
    uint8_t len = sizeof(buf);//reply data length

    if (rf95.waitAvailableTimeout(3000))// Check If there is reply in 3 seconds.
    {
        // Should be a reply message for us now   
        if (rf95.recv(buf, &len))//check if reply message is correct
       {
            if(buf[0] == 1||buf[1] == 1||buf[2] ==1) // Check if reply message has the our node ID
           {
               pinMode(4, OUTPUT);
               digitalWrite(4, HIGH);
               Serial.print("got reply: ");//print reply
               Serial.println((char*)buf);
              
               delay(400);
               digitalWrite(4, LOW); 
               //Serial.print("RSSI: ");  // print RSSI
               //Serial.println(rf95.lastRssi(), DEC);        
           }    
        }
        else
        {
           Serial.println("recv failed");//
           rf95.send(sendBuf, strlen((char*)sendBuf));//resend if no reply
        }
    }
    else
    {
        Serial.println("No reply, is rf95_server running?");//No signal reply
        rf95.send(sendBuf, strlen((char*)sendBuf));//resend data
    }
    delay(30000); // Send sensor data every 30 seconds
}



