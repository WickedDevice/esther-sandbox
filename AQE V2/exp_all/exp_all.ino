/*************************
*  Test version of AQE code
*  All sensors are connected (including DHT22), posting to Xively
*  Using external 18-bit ADC
*  Prints status messages to LCD screen
*
*
**************************/


#include <WildFire_CC3000.h>
#include <SPI.h>
#include <TinyWatchdog.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>
#include <LiquidCrystal.h>
#include <MCP3424.h>
#include <Wire.h>
#include <DHT22.h>
#include "MemoryLocations.h"
//#include <SD.h>
WildFire wf;


#define soft_reset()        \
do                          \
{                           \
    wdt_enable(WDTO_15MS);  \
    for(;;)                 \
    {                       \
    }                       \
} while(0)

void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init(void)
{
    MCUSR = 0;
    wdt_disable();
    return;
}


WildFire_CC3000 cc3000;
int sm_button = 5;

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Xively parameters
#define WEBSITE  "api.xively.com"
#define API_key  "aNMDa3DHcW9XqQUmuFQ7HrbKLqcmVO1dHBWPAQRcng9qHhft"
#define feedID   "1293806464"
#define FEED_NAME "CO_sensor"

volatile byte stat;
int reset_flag = 0;
int time = 0;
int liveness = 0;
LiquidCrystal lcd(4, 3, 24, 25, 30, 31);
uint32_t ip;
WildFire_CC3000_Client client;

#define CO_vgas_36 1
#define CO_vref_36 2
#define CO_vgas_35 3
#define CO_vref_35 4
#define O3_vgas_4 2
#define O3_vref_4 1
#define NO2_vgas_1 1
#define NO2_vref_1 2
#define NO2_vgas_2 3
#define NO2_vref_2 4
#define O3_vgas_6 3
#define O3_vref_6 4

MCP3424 MCP6(6);
MCP3424 MCP4(4);
MCP3424 MCP0(0);

/*
MCP 0
	Ch1	O3_4 vref
	Ch2	O3_4 vgas
	Ch3	CO_35 vgas
	Ch4	CO_35 vref
MCP 4
	Ch1	NO2_1 vgas
	Ch2	NO2_1 vref
	Ch3	NO2_2 vgas
	Ch4	NO2_2 vref
MCP 6
	Ch1	CO_36 vgas
	Ch2	CO_36 vref
	Ch3	O3_6 vgas
	Ch4	O3_6 vref
*/

//TODO: put these in EEPROM
#define CO_36_M 0.000212
#define CO_35_M 0.0002
#define NO2_1_M -0.0149
#define NO2_2_M -0.0157
#define O3_4_M -0.0163
#define O3_6_M -0.0172

int flag = 0;
DHT22 myDHT22(26);

//int card = 0;
//File myFile;
TinyWatchdog tinyWDT(14);
unsigned long previousMillis = 0;
long interval = 4000;
#define norm_int 20000
#define smcfg_int 60000

void setup(void)
{  
  wf.begin();
  Serial.begin(115200);
  lcd.begin(16,2);
  lcd.clear();
  uint8_t smc_status = eeprom_read_byte((const uint8_t*)tinyWDT_STATUS);
  if (smc_status == 0x37) {
    //Smartconfig was initiated, so attempt to create new connection
    tinyWDT.begin(1000, smcfg_int);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0);
    while(!attemptSmartConfigCreate());
    lcd_print_top("Connecting...");
    delay(1000);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0x5b);
    tinyWDT.force_reset();
    delay(1000);
  }
  else if (smc_status == 0x5b) {
    //New smartconfig connection was saved, so skip smartconfig timeout
    tinyWDT.begin(1000, norm_int);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0); 
    liveness = 0;
    pinMode(sm_button, INPUT_PULLUP);
    time = 10;
    lcd_print_top("Reconnecting...");
  }
  else {
    //Startup as normal
    tinyWDT.begin(1000, norm_int); 
    liveness = 0;
    pinMode(sm_button, INPUT_PULLUP);
    time = 0;
    lcd_print_top("Push button for \n");
    lcd_print_bottom("Smartconfig");
  }

  while (time < 10) {
    //wait for 10 seconds for user to select smartconfig 
    if (!digitalRead(sm_button)) {
      eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0x37);
      time = 10;
      tinyWDT.force_reset();
      delay(1000);
    }
    else {
      time++;
      if (time%2 == 0) {
        tinyWDT.pet();
      }
        lcd.setCursor(15,1);
        lcd.print(10 - time);
        delay(1000);
      }
      if (time == 10) {
        lcd_print_top("Reconnecting...");
      }
    }
  if (time >= 10) {  
    //Serial.println(F("Attempting to reconnect"));
    if(!attemptSmartConfigReconnect()){
      lcd_print_bottom("Failed!");      //Serial.println(F("Reconnect failed!"));
      cc3000.disconnect();
      delay(1000);
      _reset();
      //soft_reset();
    }
  }
  time = 0;
  
  //while(!displayConnectionDetails());
  
  // Get the website IP & print it
  ip = 0;
  // 173.203.98.29
  ip = 29;
  ip |= (((uint32_t) 98) << 8);
  ip |= (((uint32_t) 203) << 16);
  ip |= (((uint32_t) 173) << 24);

   //wdt_reset();   
   //TODO: add code to check for presence of sensors
//  cc3000.printIPdotsRev(ip);
  //Serial.println();
  lcd_print_top("Initializing");
  lcd_print_bottom("sensors... 15:00");
  delay(1000);
  //15 min sensor warmup
  int time = 0;
  while (time < 9) { //TODO: change back to 900 seconds
  if (time%2) {
    tinyWDT.pet();
  }
    //wdt_reset();
     //read data, post to xively
     lcd.setCursor(11,1);
    if((14 - time/60) < 10) {
      lcd.print(" ");
    }
    lcd.print(14 - time/60); lcd.print(":"); 
    if ((59 - time%60) < 10) {
      lcd.print(0);
    }
    lcd.print(59 - time%60);
     time++;
     delay(1000);
  }
 /* 
  if (SD.begin(4)) {
    card = 1;
    myFile = SD.open("data.txt", FILE_WRITE);
    myFile.println(F("Time (ms), CO_35 (ppm), CO_36 (ppm), NO2_1 (ppm), NO2_2 (ppm), O3_4 (ppm), O3_6 (ppm), Temperature (C), Humidity (\%)"));
    Serial.println(F("Time (ms), CO_35 (ppm), CO_36 (ppm), NO2_1 (ppm), NO2_2 (ppm), O3_4 (ppm), O3_6 (ppm), Temperature (C), Humidity (\%)"));
    myFile.close();
  }
  else {
    card = 0;
  }
  */
  lcd_print_top("Ready to begin");
  lcd_print_bottom("collecting data!");
  //wdt_reset();
}

char packet_buffer[2048];
#define BUFSIZE 511

void loop() {
  unsigned long currentMillis = millis();
  //Serial.print("Loop: "); Serial.println(currentMillis - previousMillis);
  if (currentMillis - previousMillis > interval) {
    
    previousMillis = currentMillis;
    tinyWDT.pet();
  }
  liveness++;
  //wdt_reset();
  //lcd.clear();
  //lcd.print("Connecting....");
  //wdt_disable();
  client = cc3000.connectTCP(ip, 80);
  //lcd.setCursor(0,1);
  //lcd.print("Connected!");
  //wdt_enable(WDTO_8S);
  //Serial.print("1");
  //wdt_reset();
  //read
  //directly from sensor:
  //double vref = analogReadWrapped(0)*(2.56 /1023.0);
  //delay(100);
  //double vgas = analogReadWrapped(1)*(2.56 /1023.0);
  //delay(100);

  //from ADC
  //TODO: check for presence of sensor
  unsigned long currTime = millis();
  double CO_36_vgas = getADCReading6(CO_vgas_36);
  double CO_36_vref = getADCReading6(CO_vref_36);
  double CO_36 = getGasConc(CO_36_vgas, CO_36_vref, CO_36_M);
  /*
  Serial.println("CO_36: ");
  Serial.print("Vgas = "); printDouble(CO_36_vgas, 3);
  Serial.print("Vref = "); printDouble(CO_36_vref, 3);
  Serial.print("Vdiff = "); printDouble(CO_36_vgas - CO_36_vref, 3);
  Serial.print("Current value = "); printDouble(CO_36, 3);
  */
  double CO_35_vgas = getADCReading0(CO_vgas_35);
  double CO_35_vref = getADCReading0(CO_vref_35);
  double CO_35 = getGasConc(CO_35_vgas, CO_35_vref, CO_35_M);
  /*
  Serial.println("CO_35: ");
  Serial.print("Vgas = "); printDouble(CO_35_vgas, 3);
  Serial.print("Vref = "); printDouble(CO_35_vref, 3);
  Serial.print("Vdiff = "); printDouble(CO_35_vgas - CO_35_vref, 3);
  Serial.print("Current value = "); printDouble(CO_35, 3);
  */
  double NO2_1_vgas = getADCReading4(NO2_vgas_1);
  double NO2_1_vref = getADCReading4(NO2_vref_1);
  double NO2_1 = getGasConc(NO2_1_vgas, NO2_1_vref, NO2_1_M);
  /*
  Serial.println("NO2_1: ");
  Serial.print("Vgas = "); printDouble(NO2_1_vgas, 3);
  Serial.print("Vref = "); printDouble(NO2_1_vref, 3);
  Serial.print("Vdiff = "); printDouble(NO2_1_vgas - NO2_1_vref, 3);
  Serial.print("Current value = "); printDouble(NO2_1, 3);
  */
  double NO2_2_vgas = getADCReading4(NO2_vgas_2);
  double NO2_2_vref = getADCReading4(NO2_vref_2);
  double NO2_2 = getGasConc(NO2_2_vgas, NO2_2_vref, NO2_2_M);
  /*
  Serial.println("NO2_2: ");
  Serial.print("Vgas = "); printDouble(NO2_2_vgas, 3);
  Serial.print("Vref = "); printDouble(NO2_2_vref, 3);
  Serial.print("Vdiff = "); printDouble(NO2_2_vgas - NO2_2_vref, 3);
  Serial.print("Current value = "); printDouble(NO2_2, 3);
  */
  double O3_4_vgas = getADCReading0(O3_vgas_4);
  double O3_4_vref = getADCReading0(O3_vref_4);
  double O3_4 = getGasConc(O3_4_vgas, O3_4_vref, O3_4_M);
  /*
  Serial.println("O3_4: ");
  Serial.print("Vgas = "); printDouble(O3_4_vgas, 3);
  Serial.print("Vref = "); printDouble(O3_4_vref, 3);
  Serial.print("Vdiff = "); printDouble(O3_4_vgas - O3_4_vref, 3);
  Serial.print("Current value = "); printDouble(O3_4, 3);
  */
  double O3_6_vgas = getADCReading6(O3_vgas_6);
  double O3_6_vref = getADCReading6(O3_vref_6);
  double O3_6 = getGasConc(O3_6_vgas, O3_6_vref, O3_6_M);
  /*
  Serial.println("O3_6: ");
  Serial.print("Vgas = "); printDouble(O3_6_vgas, 3);
  Serial.print("Vref = "); printDouble(O3_6_vref, 3);
  Serial.print("Vdiff = "); printDouble(O3_6_vgas - O3_6_vref, 3);
  Serial.print("Current value = "); printDouble(O3_6, 3);
  */
  double temp;
  double hum;
  DHT22_ERROR_t errorCode = myDHT22.readData();
  char tempbuf_bot[17] = "";
  Serial.println(errorCode);
  switch(errorCode)
  {
    case DHT_ERROR_NONE: {
      temp = myDHT22.getTemperatureC();
      hum = myDHT22.getHumidity();
      break;
    }
    case DHT_ERROR_CHECKSUM: {
      sprintf(tempbuf_bot, "checksum error ");
      break;
    }
    case DHT_BUS_HUNG: {
      sprintf(tempbuf_bot, "    BUS Hung ");
      break;
    }
    case DHT_ERROR_NOT_PRESENT: {
      sprintf(tempbuf_bot, "  Not Present ");
      break;
    }
    case DHT_ERROR_ACK_TOO_LONG: {
      sprintf(tempbuf_bot, "  ACK time out ");
      break;
    }
    case DHT_ERROR_SYNC_TIMEOUT: {
      sprintf(tempbuf_bot, "  Sync Timeout ");
      break;
    }
    case DHT_ERROR_DATA_TIMEOUT: {
      sprintf(tempbuf_bot, "  Data Timeout ");
      break;
    }
    case DHT_ERROR_TOOQUICK: {
      sprintf(tempbuf_bot, " Polled too quick ");
      break;
    }
  }
  //Serial.print("2");
  //wdt_reset();
  //post
  int datalength = 0;
  #define DATA_MAX_LENGTH 512
  char data[DATA_MAX_LENGTH] = "\n";
  char SDdata[DATA_MAX_LENGTH];
  /*
  if (card) {
    sprintf(SDdata, "%lu", currTime);
    strcat_P(SDdata, PSTR(","));
  }
  */
  strcat_P(data, PSTR("{\"version\":\"1.0.0\",\"datastreams\" : ["));
  char dbuf[10] = "";
  switch (flag%3) {
  case 0:
      strcat(data, "{\"id\" : \"CO_36\",\"current_value\" : \"");
      strcat(data,dtostrf(CO_36, 5, 3, dbuf)); 
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_36_vgas\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_36_vgas, 5, 3, dbuf)); 
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_36_vref\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_36_vref, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_36_vdiff\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_36_vgas - CO_36_vref, 5, 3, dbuf));
      strcat_P(data, PSTR("\"},"));

      strcat_P(data, PSTR("{\"id\" : \"CO_35\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_35, 5, 3, dbuf)); 
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_35_vgas\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_35_vgas, 5, 3, dbuf)); 
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_35_vref\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_35_vref, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"CO_35_vdiff\",\"current_value\" : \""));
      strcat(data, dtostrf(CO_35_vgas - CO_35_vref, 5, 3, dbuf));
      strcat_P(data, PSTR("\"},"));
      break;

  case 1:
      strcat_P(data, PSTR("{\"id\" : \"NO2_1\",\"current_value\" : \""));
      strcat(data, dtostrf(NO2_1, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"NO2_1_vgas\",\"current_value\" : \""));
      strcat(data, dtostrf(NO2_1_vgas, 5, 3, dbuf)); 
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"NO2_1_vref\",\"current_value\" : \""));
      strcat(data, dtostrf(NO2_1_vref, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"NO2_1_vdiff\",\"current_value\" : \""));
      strcat(data, dtostrf(NO2_1_vgas - NO2_1_vref, 5, 3, dbuf));
     strcat_P(data, PSTR("\"},"));

    strcat_P(data, PSTR("{\"id\" : \"NO2_2\",\"current_value\" : \""));
    strcat(data, dtostrf(NO2_2, 5, 3, dbuf));
    
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"NO2_2_vgas\",\"current_value\" : \""));
    strcat(data, dtostrf(NO2_2_vgas, 5, 3, dbuf)); 
    
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"NO2_2_vref\",\"current_value\" : \""));
    strcat(data, dtostrf(NO2_2_vref, 5, 3, dbuf));
    
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"NO2_2_vdiff\",\"current_value\" : \""));
    strcat(data, dtostrf(NO2_2_vgas - NO2_2_vref, 5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
    break;
    case 2:

      strcat_P(data, PSTR("{\"id\" : \"O3_4\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_4, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_4_vgas\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_4_vgas, 5, 3, dbuf)); 
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_4_vref\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_4_vref, 5, 3, dbuf));
      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_4_vdiff\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_4_vgas - O3_4_vref, 5, 3, dbuf));
      strcat_P(data, PSTR("\"},"));

      strcat_P(data, PSTR("{\"id\" : \"O3_6\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_6, 5, 3, dbuf));     
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_6_vgas\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_6_vgas, 5, 3, dbuf));      
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_6_vref\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_6_vref, 5, 3, dbuf));
      strcat_P(data, PSTR("\"},"));
      strcat_P(data, PSTR("{\"id\" : \"O3_6_vdiff\",\"current_value\" : \""));
      strcat(data, dtostrf(O3_6_vgas - O3_6_vref, 5, 3, dbuf));
      strcat_P(data, PSTR("\"},"));
      break;
  }
      if (errorCode == DHT_ERROR_NONE) {
        
        strcat_P(data, PSTR("{\"id\" : \"Temperature\",\"current_value\" : \""));
        strcat(data, dtostrf(temp, 5, 3, dbuf));
        strcat_P(data, PSTR("\"},"));
        /*
        if (card) {
          strcat(SDdata, dbuf);
          strcat_P(SDdata, PSTR(","));
        }
        */
        strcat_P(data, PSTR("{\"id\" : \"Humidity\",\"current_value\" : \""));
        strcat(data, dtostrf(hum, 5, 3, dbuf));
        strcat_P(data, PSTR("\"},"));
        /*
        if (card) {
          strcat(SDdata, dbuf);
          strcat_P(SDdata, PSTR("\n"));
        }
        */
      }
      
      
  strcat_P(data, PSTR("{\"id\" : \"Liveness\",\"current_value\" : \""));
  itoa(liveness, dbuf, DEC);
  strcat(data, dbuf);
  strcat_P(data, PSTR("\"}]}"));
  //lcd.print("2");
  datalength = strlen(data);
  /*
  Serial.print("Data length");
  Serial.println(datalength);
  Serial.println();
  */
  //wdt_reset();
  //Serial.print("3");
  char putstr_buffer[64] = "PUT /v2/feeds/";
  strcat_P(putstr_buffer, PSTR(feedID));
  strcat_P(putstr_buffer, PSTR(".json HTTP/1.0"));
  //Serial.print("putstr_buffer: "); Serial.println(putstr_buffer);
  char apikey_buffer[128] = "X-ApiKey: ";
  strcat_P(apikey_buffer, PSTR(API_key));
  //Serial.print("apikey_buffer: "); Serial.println(apikey_buffer);
  //lcd.print("3");
  char contentlen_buffer[64] = "Content-Length: ";
  char len_buffer[32] = "";
  itoa(datalength, len_buffer, 10);
  strcat(contentlen_buffer, len_buffer); 

  //wdt_reset();    
  if (client.connected()) {
    //Serial.print("4");
    //lcd.print("4");
    packet_buffer[0] = '\0';    
    strcat(packet_buffer, putstr_buffer);
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat_P(packet_buffer, PSTR("Host: api.xively.com\n"));
    strcat(packet_buffer, apikey_buffer);
    //lcd.print("5");
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat(packet_buffer, contentlen_buffer);
    strcat_P(packet_buffer, PSTR("\n"));  
    strcat_P(packet_buffer, PSTR("Connection: close\n"));   
    strcat(packet_buffer, data);

    //wdt_reset();      
    //lcd.print("6 ");
    
    //lcd.print(strlen(packet_buffer));
    //for (int i = 0; i < strlen(packet_buffer); i++) {
      //Serial.println(packet_buffer);
      client.fastrprint(packet_buffer);
        
        //wdt_reset();
    //}
    //Serial.println();
    client.println();
    
    //lcd.print("7");    
  } else {
    //Serial.print("5");
    lcd_print_top("Could not post ");
    lcd_print_bottom("to Xively!");
    Serial.println(F("Connection failed")); 
    client.close();
    delay(1000);   
    _reset();
  }
  /*
  if (card) {
    myFile = SD.open("data.txt", FILE_WRITE);
    if (myFile) {    
      myFile.print(SDdata);
      Serial.print(SDdata);
      myFile.close();
    }
  }
  */
  //Serial.print("6");
//Cycle through readings? loop1:CO loop2:NO2 loop3:O3 loop4:temp loop5:hum  
switch (flag) {
  case 0: {  
    lcd_print_top("    CO value");
    lcd.setCursor(4,1);
    lcd.print(CO_36); lcd.print(" ppm(1)");
    break;
  }
  case 1: {  
    lcd_print_top("    CO value");
    lcd.setCursor(4,1);
    lcd.print(CO_35); lcd.print(" ppm(2)");
    break;
  }
  case 2: {
    lcd_print_top("   NO2 value");
    lcd.setCursor(4,1);
    lcd.print(NO2_1); lcd.print(" ppm(1)");
    break;
  }
  case 3: {
    lcd_print_top("   NO2 value");
    lcd.setCursor(4,1);
    lcd.print(NO2_2); lcd.print(" ppm(2)");
    break;
  }
  case 4: {
    lcd_print_top("    O3 value");
    lcd.setCursor(4,1);
    lcd.print(O3_4); lcd.print(" ppm(1)");
    break;
  }
  case 5: {
    lcd_print_top("    O3 value");
    lcd.setCursor(4,1);
    lcd.print(O3_6); lcd.print(" ppm(2)");
    break;
  }      
  case 6: {
    lcd_print_top("  Temperature");
    if (errorCode == DHT_ERROR_NONE) {
      lcd.setCursor(4,1);
      lcd.print(temp); lcd.print(" C");
    }
    else {
      lcd_print_bottom(tempbuf_bot);
    }
    break;
  }
  case 7: {
    lcd_print_top("    Humidity");
    if (errorCode == DHT_ERROR_NONE) {
      lcd.setCursor(5,1);
      lcd.print(hum); lcd.print(" %");
    }
    else {
      lcd_print_bottom(tempbuf_bot);
    }
    break;
  }
  
}
flag++;
flag %= 8;
  //Serial.print("Before read: "); Serial.println(millis() - previousMillis);
    //Serial.println(F("-------------------------------------"));
    //wdt_reset();   
  // t = 0;
  //t2 = 0;
 //ttotal = 0; 
 //int counter = 0;
    while (client.connected()) {
      //wdt_reset();      
      //t = millis(); 
      while (client.available()) {     
        //wdt_reset();     
          
        char c = client.read();
        //counter++;
        //Serial.print(c);
      }
      //t2 = millis() - t;
        //ttotal += t2;
        //if (t2 > tmax) {
          //tmax = t2;
          //Serial.print ("New max read time : "); Serial.println(tmax);
        //}
        //Serial.print("After available :"); Serial.println(millis() - previousMillis);
    }  
    //wdt_reset(); 
   //Serial.print("After read: "); Serial.println(millis() - previousMillis); 
    client.close();
    //Serial.print("Read + available took "); Serial.print(ttotal); Serial.println(" ms");
    //Serial.print(counter); Serial.println(" chars were read.");
    delay(1000);
    
}

void lcd_print_top(char* string) {
  lcd.clear();
  lcd.print(string);
}

void lcd_print_bottom(char* string) {
  lcd.setCursor(0,1);
  lcd.print(string);
}

double getADCReading6(int channel) {
  //Voffset = 0?
  //for 101713-46 (0.000446)
  //double v = (vgas - vref)/(.000446);
  //Serial.print("2");
  //wdt_disable();
  MCP6.Configuration(channel,18,0,1);
  //Serial.print("3");
  //wdt_reset();
  MCP6.NewConversion();
  //Serial.print("4");
  //wdt_disable();
  //wdt_reset();
  double v = MCP6.Measure();
  //Serial.print("Voltage = "); // print result
  //Serial.print(v);
  //Serial.println(" microVolt");
  //Serial.print("5");
  //wdt_enable(WDTO_8S);
  //wdt_reset();
  v /= 1000000.0;
  //v /= M;
  //Serial.print("Concentration = ");
  //Serial.print(v);
  //Serial.println(" ppm");
  //if (v < 2) {
  //  return 0.00;
  //}
  return v;
}

double getADCReading4(int channel){ //double vgas, double vref) {
  //Voffset = 0?
  //for 101713-46 (0.000446)
  //double v = (vgas - vref)/(.000446);
  //Serial.print("2");
  //wdt_disable();
  MCP4.Configuration(channel,18,0,1);
  //Serial.print("3");
  //wdt_reset();
  MCP4.NewConversion();
  //Serial.print("4");
  //wdt_disable();
  //wdt_reset();
  double v = MCP4.Measure();
  //Serial.print("Voltage = "); // print result
  //Serial.print(v);
  //Serial.println(" microVolt");
  //Serial.print("5");
  //wdt_enable(WDTO_8S);
  //wdt_reset();
  v /= 1000000.0;
  //v /= M;
  //Serial.print("Concentration = ");
  //Serial.print(v);
  //Serial.println(" ppm");
  //if (v < 2) {
  //  return 0.00;
  //}
  return v;
}

double getADCReading0(int channel) {
  MCP0.Configuration(channel, 18,0,1);
  MCP0.NewConversion();
  double v = MCP0.Measure();
  v /= 1000000.0;
  return v;
}

//#nofilter #yolo
double getGasConc(double vgas, double vref, double M) {
 return (vgas - vref)/M; 
}

/*
bool displayConnectionDetails(void) {
  uint32_t addr, netmask, gateway, dhcpserv, dnsserv;

  if(!cc3000.getIPAddress(&addr, &netmask, &gateway, &dhcpserv, &dnsserv))
    return false;

  Serial.print(F("IP Addr: ")); cc3000.printIPdotsRev(addr);
  Serial.print(F("\r\nNetmask: ")); cc3000.printIPdotsRev(netmask);
  Serial.print(F("\r\nGateway: ")); cc3000.printIPdotsRev(gateway);
  Serial.print(F("\r\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
  Serial.print(F("\r\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
  Serial.println();
  return true;
}
*/
boolean attemptSmartConfigReconnect(void){
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
  /* !!! Note the additional arguments in .begin that tell the   !!! */
  /* !!! app NOT to deleted previously stored connection details !!! */
  /* !!! and reconnected using the connection details in memory! !!! */
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */  
  if (!cc3000.begin(false, true))
  {
    lcd_print_top("Can’t connect wi");
    lcd_print_bottom("th saved details");
    //Serial.println(F("Unable to re-connect!? Did you run the SmartConfigCreate"));
    //Serial.println(F("sketch to store your connection details?"));
    return false;
  }

  /* Round of applause! */
  lcd_print_top("Reconnected!");
  //Serial.println(F("Reconnected!"));
  
  /* Wait for DHCP to complete */
  lcd_print_bottom("Requesting DHCP");
  //Serial.println(F("\nRequesting DHCP"));
  time = 0;
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
    time += 100;
    if (time%2000 == 0) {
      tinyWDT.pet();
    }
    //wdt_reset();
    if (time > 30000) {
      lcd_print_top("DHCP failed!");
      delay(500);
      //Serial.println(F("DHCP failed!"));
      return false;
    }
  }  
  return true;
}

boolean attemptSmartConfigCreate(void){
  /* Initialise the module, deleting any existing profile data (which */
  /* is the default behaviour)  */
  //Serial.println(F("\nInitialising the CC3000 ..."));
  lcd_print_top("Creating new ");
  lcd_print_bottom("connection...");
  if (!cc3000.begin(false))
  {
    return false;
  }  
  
  /* Try to use the smart config app (no AES encryption), saving */
  /* the connection details if we succeed */
  //Serial.println(F("Waiting for a SmartConfig connection (~60s) ..."));
  if (!cc3000.startSmartConfig(false))
  {
    lcd_print_top("Failed!");
    delay(1000);
    //Serial.println(F("SmartConfig failed"));
    return false;
  }
  
 // Serial.println(F("Saved connection details and connected to AP!"));
  lcd_print_top("Connection saved!");  
  
  return true;
}

void _reset(void) {
  Serial.println("Oh no!");
   time = 10000;
   while (time > 0) {
      lcd_print_top("Resetting in ");
      lcd.setCursor(0,1);
      lcd.print(time/1000); lcd.print(" seconds...");
      time -=1000;
      delay (1000);
   }
   lcd_print_top("Resetting now...");
   for(;;);
   //soft_reset();
}
void printDouble( double val, byte precision){
// prints val with number of decimal places determine by precision
// NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
// example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)
  if (val < 0 && val > -1) {
    Serial.print("-");
  }
Serial.print (int(val));  //prints the int part
  if( precision > 0) {
    Serial.print("."); // print the decimal point
    unsigned long frac;
    unsigned long mult = 1;
    byte padding = precision -1;
    while(precision--)
       mult *=10;
       
    if(val >= 0)
      frac = (val - int(val)) * mult;
    else
      frac = (int(val)- val ) * mult;
    unsigned long frac1 = frac;
    while( frac1 /= 10 )
      padding--;
    while(  padding--)
      Serial.print("0");
    Serial.print(frac,DEC) ;
  }
  Serial.println();
} 
