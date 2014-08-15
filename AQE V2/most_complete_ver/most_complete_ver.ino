/*************************
*  First version of AQE code
*  All sensors are connected (including DHT22), posting to Xively
*  Using external 18-bit differential ADC
*  Prints status messages to LCD screen
*  TODO: Add local data storage option (SD card) -- drop for now?
*  TODO: Add code to check for presence of sensor -- test
*  TODO: Add detection for sensor warmup period (need MCUSR)
**************************/

#include <SPI.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>
#include <WildFire_CC3000.h>
#include <LiquidCrystal.h>
#include <MCP3424.h>
#include <Wire.h>
#include <DHT22.h>
#include <avr/eeprom.h>
#include <TinyWatchdog.h>
#include <lmp91000.h>
#include "MemoryLocations.h"

#define DUST_SENSOR 1

WildFire wf;
WildFire_CC3000 cc3000;
WildFire_CC3000_Client client;
int sm_button = A0;
DHT22 myDHT22(A1);
LiquidCrystal lcd(A3,A2, 4,5,6,8);
MCP3424 MCP(0);
TinyWatchdog tinyWDT;
uint8_t mymac[6];

#define TIACN_REG_VAL 0x1c
#define CO_REFCN 0x91
#define NO2_REFCN 0xd5
#define O3_REFCN 0xd1
#define MODECN_REG_VAL 0x03

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Xively parameters
#define WEBSITE  "api.xively.com"

int time = 0;
uint32_t ip;

double CO_M;
double NO2_M;
double O3_M;
const long magic_number = 0x5485; //must match the magic number in the provisioning sketch
#define CO_MENB 7
#define NO2_MENB 9
#define O3_MENB 10
LMP91000 CO_LMP(CO_MENB);
LMP91000 NO2_LMP(NO2_MENB);
LMP91000 O3_LMP(O3_MENB);
uint8_t CO_PRESENT = 0;
uint8_t NO2_PRESENT = 0;
uint8_t O3_PRESENT = 0;

#define particulate1 2
#define particulate2 3

#define PROVISIONING_STATUS_GOOD 0x73
char api_key[API_KEY_LENGTH];
char feedID[FEED_ID_LENGTH];
int period = 60000; //delay period, default is one minute

int flag = 0;
void initialize_eeprom(void);
void printDouble(double val, byte precision);

unsigned long previousMillis = 0;
long interval = 4000;
int liveness = 0;
#define norm_int 20000
#define smcfg_int 60000
/*
tinyWDT_STATUS: 0x0 = initial state; 0x37 = initiate smartconfig; 0x5b = skip smartconfig timer (and initiate sensor warmup); 
*/

char packet_buffer[2048];
#define BUFSIZE 511

void setupTimer2Interrupt(void);
void updateCounts(void);
uint32_t num_ones1 = 0;
uint32_t num_zeros1 = 0;
uint32_t num_ones2 = 0;
uint32_t num_zeros2 = 0;

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
    time = 11;
    lcd_print_top("Connecting...");
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
      //Allow user to initiate resetting calibration values via serial monitor
      if (Serial.available()) {
        char c = Serial.read();
        initialize_eeprom();
        Serial.println("Resetting now...");
        lcd_print_top("Resetting now...");
        delay(1000);
        tinyWDT.force_reset();
        delay(1000);
      }
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
    if(!attemptSmartConfigReconnect()){
      lcd_print_bottom("Failed!");
      cc3000.disconnect();
      delay(1000);
      _reset();
    }
  }
  time = 0;
  
  if (!cc3000.getMacAddress(mymac)) {
  	Serial.println("Could not get MAC address!");
  	while (1); //TODO: is this correct behavior?
   }
 
  char serial_number[18] = {0};
  Serial.println();
  Serial.println(F("**********************************************************************************"));
  Serial.print(F("Serial #: "));
  for(uint8_t ii = 0; ii < 6; ii++){
    convertByteArrayToAsciiHex(mymac + ii, serial_number + 3*ii, 1);
    if(ii == 5) serial_number[3*ii+2] = '\0';
    else serial_number[3*ii+2] = ':';
  }
  Serial.println(serial_number);  
  Serial.println(F("**********************************************************************************"));
  
  // Get the website IP
  ip = 0;
  while(ip == 0) {
    cc3000.getHostByName(WEBSITE, &ip);
  }
  
  //Get stored API key, feed ID, and calibration data
  uint8_t test = eeprom_read_byte((const uint8_t *) ACTIVATION_STATUS_EEPROM_ADDRESS);
  if(test != PROVISIONING_STATUS_GOOD){
    for(;;) {
      lcd_print_top("This egg is not ");
      lcd_print_bottom("   activated!");
      delay(2000);
      lcd_print_top(" Please contact");
      lcd_print_bottom(" Wicked Device");
      delay(2000);
      tinyWDT.pet();
    }
  }
  eeprom_read_block(api_key, (const void*)API_KEY_EEPROM_ADDRESS, API_KEY_LENGTH);
  eeprom_read_block(feedID, (const void*)FEED_ID_EEPROM_ADDRESS, FEED_ID_LENGTH); 
  long magic;
  eeprom_read_block(&magic, (const void*)CAL_MAGIC, sizeof(float));
  if(magic != magic_number) {
    for (;;) {
      lcd_print_top("Missing sensor");
      lcd_print_bottom("calibration data!");
      delay(2000);
      lcd_print_top(" Please contact");
      lcd_print_bottom(" Wicked Device");
      delay(2000);
      tinyWDT.pet();
    }
  }

  //read all constants whether they are there or not
  //if the sensor module is not detected, the value will not be used anyways
  eeprom_read_block(&CO_M, (const void*)CO_cal, sizeof(float));
  eeprom_read_block(&NO2_M, (const void*)NO2_cal, sizeof(float));
  eeprom_read_block(&O3_M, (const void*)O3_cal, sizeof(float));
  
  pinMode(CO_MENB, OUTPUT);
  pinMode(NO2_MENB, OUTPUT);
  pinMode(O3_MENB, OUTPUT);
  
  //configure LMP91000 and check for sensor module presence
  //menb is now active high (I2C duplicator)
  digitalWrite(CO_MENB, HIGH);
  if (CO_LMP.begin(TIACN_REG_VAL, CO_REFCN, MODECN_REG_VAL)) {
    CO_PRESENT = 1;
  }
  digitalWrite(CO_MENB, LOW);
  delay(100);
  digitalWrite(NO2_MENB, HIGH);
  if (NO2_LMP.begin(TIACN_REG_VAL, NO2_REFCN, MODECN_REG_VAL)) {
    NO2_PRESENT = 1;
  }
  digitalWrite(NO2_MENB, LOW);
  delay(100);
  digitalWrite(O3_MENB, HIGH);
  if (O3_LMP.begin(TIACN_REG_VAL, O3_REFCN, MODECN_REG_VAL)) {
    O3_PRESENT = 1;
  }
  digitalWrite(O3_MENB, LOW);
  
#if DUST_SENSOR  
  //configure particulate sensor (later will be replaced by global initialization)
  pinMode(particulate1, INPUT);
  pinMode(particulate2, INPUT);
#endif

//post to Xively to indicate liveness
  post_liveness();
  //TODO: this conditional statement depends on the value of MCUSR and tinyWDT status
  uint8_t mcusr_stat = eeprom_read_byte((const uint8_t*)MCUSR_ADDRESS);
  if ((mcusr_stat & 1) || (smc_status == 0x5b)) {
    lcd_print_top("Initializing");
    lcd_print_bottom("sensors... 15:00");
    delay(1000);
    //15 min sensor warmup
    time = 0;
    while (time < 900) {
      if (time%2 == 0) {
        tinyWDT.pet();
      }
      if (time % 60 == 0) {
        post_liveness();
      }
      lcd_print_top("Initializing");
      lcd_print_bottom("sensors...");
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
  }
  lcd_print_top("Ready to begin");
  lcd_print_bottom("collecting data!");
}



void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    tinyWDT.pet();
  }
  
  //update reading from dust sensor
#if DUST_SENSOR  
  cli();
  double a1 = (double)num_zeros1/ 100.0;
  double a2 = (double)num_zeros2 / 100.0;
  sei();
  updateCounts();
#endif

  client = cc3000.connectTCP(ip, 80);

  //get sensor readings
  int currTime = millis();
  double CO;
  double NO2;
  double O3;
  if (CO_PRESENT) {
    CO = getGasConc(CO_MENB, CO_M, 2.0); 
  }
  if (NO2_PRESENT) {
    NO2 = getGasConc(NO2_MENB, NO2_M, 0.3);
  }
  if (O3_PRESENT) {
    O3 = getGasConc(O3_MENB,  O3_M, 0.3);
  }
  double temp;
  double hum;
  DHT22_ERROR_t errorCode = myDHT22.readData();
  char tempbuf[17] = "";
  //Read from DHT22 if no error
  switch(errorCode)
  {
    case DHT_ERROR_NONE: {
      temp = myDHT22.getTemperatureC();
      hum = myDHT22.getHumidity();
      break;
    }
    case DHT_ERROR_CHECKSUM: {
      sprintf(tempbuf, "checksum error ");
      break;
    }
    case DHT_BUS_HUNG: {
      sprintf(tempbuf, "    BUS Hung ");
      break;
    }
    case DHT_ERROR_NOT_PRESENT: {
      sprintf(tempbuf, "  Not Present ");
      break;
    }
    case DHT_ERROR_ACK_TOO_LONG: {
      sprintf(tempbuf, "  ACK time out ");
      break;
    }
    case DHT_ERROR_SYNC_TIMEOUT: {
      sprintf(tempbuf, "  Sync Timeout ");
      break;
    }
    case DHT_ERROR_DATA_TIMEOUT: {
      sprintf(tempbuf, "  Data Timeout ");
      break;
    }
    case DHT_ERROR_TOOQUICK: {
      sprintf(tempbuf, " Polled too quick ");
      break;
    }
  }
  
  //post
  int datalength = 0;
  char dbuf[10] = "";
  #define DATA_MAX_LENGTH 512
  char data[DATA_MAX_LENGTH] = "\n";
  
  //KWJ sensor
  strcat_P(data, PSTR("{\"version\":\"1.0.0\",\"datastreams\" :"));
  if (CO_PRESENT) {
    strcat_P(data, PSTR("[{\"id\" : \"CO\",\"current_value\" : \"")); 
    strcat(data,dtostrf(CO, 5, 3, dbuf)); 
    strcat_P(data, PSTR("\"},"));
  }
  if (NO2_PRESENT) {
    strcat_P(data, PSTR("{\"id\" : \"NO2\",\"current_value\" : \""));
    strcat(data, dtostrf(NO2, 5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
  }
  if (O3_PRESENT) {
    strcat_P(data, PSTR("{\"id\" : \"O3\",\"current_value\" : \""));
    strcat(data, dtostrf(O3, 5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
  }
  
  //particulate  
#if DUST_SENSOR
    strcat_P(data, PSTR("{\"id\" : \"Small_particulate\",\"current_value\" : \""));
    strcat(data, dtostrf(a1, 5, 3, dbuf));  
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"Large_particulate\",\"current_value\" : \""));
    strcat(data, dtostrf(a2,5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
#endif
  
  //DHT22
  if (errorCode == DHT_ERROR_NONE) {   
    strcat_P(data, PSTR("{\"id\" : \"Temperature\",\"current_value\" : \""));
    strcat(data, dtostrf(temp, 5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"Humidity\",\"current_value\" : \""));
    strcat(data, dtostrf(hum, 5, 3, dbuf));
    strcat_P(data, PSTR("\"},"));
  }
  
  
  //liveness
  liveness++;
  strcat_P(data, PSTR("{\"id\" : \"Liveness\",\"current_value\" : \""));
  itoa(liveness, dbuf, DEC);
  strcat(data, dbuf);
  strcat_P(data, PSTR("\"}]}"));
  datalength = strlen(data);

  char putstr_buffer[64] = "PUT /v2/feeds/";
  strcat(putstr_buffer, feedID);
  strcat_P(putstr_buffer, PSTR(".json HTTP/1.0"));
  
  char apikey_buffer[128] = "X-ApiKey: ";
  strcat(apikey_buffer, api_key);

  char contentlen_buffer[64] = "Content-Length: ";
  char len_buffer[32] = "";
  itoa(datalength, len_buffer, 10);
  strcat(contentlen_buffer, len_buffer); 
  
  if (client.connected()) {
    packet_buffer[0] = '\0';    
    strcat(packet_buffer, putstr_buffer);
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat_P(packet_buffer, PSTR("Host: api.xively.com\n"));
    strcat(packet_buffer, apikey_buffer);
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat(packet_buffer, contentlen_buffer);
    strcat_P(packet_buffer, PSTR("\n"));  
    strcat_P(packet_buffer, PSTR("Connection: close\n"));   
    strcat(packet_buffer, data);
    for (int i = 0; i < strlen(packet_buffer); i++) {
        client.print(packet_buffer[i]);
        //Serial.print(packet_buffer[i]);
    }
    //Serial.println();
    client.println();   
  } else {
    lcd_print_top("Could not post ");
    lcd_print_bottom("to Xively!"); 
    client.close();
    delay(1000);   
    _reset();
  }

//Cycle through readings to display on LCD screen
//loop1:CO loop2:NO2 loop3:O3 loop4:temp loop5:hum  
int f = 5;

#if DUST_SENSOR
f = 7;
#endif

switch (flag % f) {
  case 0: {  
    lcd_print_top("    CO value");
    lcd.setCursor(4,1);
    lcd.print(CO); lcd.print(" ppm");
    break;
  }
  case 1: {
    lcd_print_top("    NO2 value");
    lcd.setCursor(4,1);
    lcd.print(NO2); lcd.print(" ppm");
    break;
  }
  case 2: {
    lcd_print_top("    O3 value");
    lcd.setCursor(4,1);
    lcd.print(O3); lcd.print(" ppm");
    break;
  }
  case 3: {
    lcd_print_top("  Temperature");
    if (errorCode == DHT_ERROR_NONE) {
      lcd.setCursor(4,1);
      lcd.print(temp); lcd.print(" C");
    }
    else {
      lcd_print_bottom(tempbuf);
    }
    break;
  }
  case 4: {
    lcd_print_top("    Humidity");
    if (errorCode == DHT_ERROR_NONE) {
      lcd.setCursor(5,1);
      lcd.print(hum); lcd.print(" %");
    }
    else {
      lcd_print_bottom(tempbuf);
    }
    break;
  }
#if DUST_SENSOR  
  case 5: {
    lcd_print_top("Small particles");
    lcd.setCursor(5,1);
    lcd.print(a1); lcd.print(" %");
    break;
  }
  case 6: {
    lcd_print_top("Large particles");
    lcd.setCursor(5,1);
    lcd.print(a2); lcd.print(" %");
    break;
  }
#endif  
}
flag++;
   
    while (client.connected()) {    
      while (client.available()) {            
        char c = client.read();
        //Serial.print(c);
      }
    }   
    client.close();
    delay(period);    
}

void lcd_print_top(char* string) {
  lcd.clear();
  lcd.print(string);
}

void lcd_print_bottom(char* string) {
  lcd.setCursor(0,1);
  lcd.print(string);
}

double getGasConc(int menb, double M, double L){
  digitalWrite(menb, HIGH);
  MCP.Configuration(0,16,1,1); //16-bit resolution, 15 SPS, continuous mode for running average
  //all MCP have same address...
  //MCP.NewConversion();
  double v = 0;
  for (int i = 0; i < 50; i++) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      tinyWDT.pet();
    }
    v += MCP.Measure();
  }
  v /= 50.0;
  MCP.Configuration(0,16,0,1); //change back to one-shot conversion to save power while other sensors are being read
  digitalWrite(menb, LOW);
  v /= 1000000.0; //convert from uV to V
  v /= M;
  if (v < L) { //filter results
    return 0.00;
  }
  return v;
}

void post_liveness() {
 client = cc3000.connectTCP(ip, 80);
  int datalength = 0;
  char dbuf[10] = "";
  #define DATA_MAX_LENGTH 512
  char data[DATA_MAX_LENGTH] = "\n";
  strcat_P(data, PSTR("{\"version\":\"1.0.0\",\"datastreams\" : ["));
  
  strcat_P(data, PSTR("{\"id\" : \"Liveness\",\"current_value\" : \""));
  itoa(liveness, dbuf, DEC);
  strcat(data, dbuf);
  strcat_P(data, PSTR("\"}]}"));
  datalength = strlen(data);
  char putstr_buffer[64] = "PUT /v2/feeds/";
  strcat(putstr_buffer, feedID);
  strcat_P(putstr_buffer, PSTR(".json HTTP/1.0"));
  
  char apikey_buffer[128] = "X-ApiKey: ";
  strcat(apikey_buffer, api_key);
  char contentlen_buffer[64] = "Content-Length: ";
  char len_buffer[32] = "";
  itoa(datalength, len_buffer, 10);
  strcat(contentlen_buffer, len_buffer); 
  if (client.connected()) {
    packet_buffer[0] = '\0';    
    strcat(packet_buffer, putstr_buffer);
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat_P(packet_buffer, PSTR("Host: api.xively.com\n"));
    strcat(packet_buffer, apikey_buffer);
    strcat_P(packet_buffer, PSTR("\n")); 
    strcat(packet_buffer, contentlen_buffer);
    strcat_P(packet_buffer, PSTR("\n"));  
    strcat_P(packet_buffer, PSTR("Connection: close\n"));   
    strcat(packet_buffer, data);
    client.fastrprint(packet_buffer);
    client.println();   
  } else {
    lcd_print_top("Could not post ");
    lcd_print_bottom("to Xively!");
    Serial.println(F("Connection failed")); 
    client.close();
    delay(1000);   
    _reset();
  }
    while (client.connected()) {
      while (client.available()) {     
        char c = client.read();
      }
    }
  client.close();
  liveness++;  
}

boolean attemptSmartConfigReconnect(void){
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
  /* !!! Note the additional arguments in .begin that tell the   !!! */
  /* !!! app NOT to deleted previously stored connection details !!! */
  /* !!! and reconnected using the connection details in memory! !!! */
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */  
  //Serial.println(1);
  if (!cc3000.begin(false, true))
  {
    lcd_print_top("Can't connect wi");
    lcd_print_bottom("th saved details");
    return false;
  }
  
  /* Round of applause! */
  lcd_print_top("Reconnected!");

  /* Wait for DHCP to complete */
  lcd_print_bottom("Requesting DHCP");
  time = 0;
  while (!cc3000.checkDHCP()) {
    delay(100);
    time += 100;
    if (time%2000 == 0) {
        tinyWDT.pet();
      }
    if (time > 20000) {
      lcd_print_top("DHCP failed!");
      delay(500);
      return false;
    }
  }  
  return true;
}


boolean attemptSmartConfigCreate(void){
  /* Initialise the module, deleting any existing profile data (which */
  /* is the default behaviour)  */
  lcd_print_top("Creating new ");
  lcd_print_bottom("connection...");
  if (!cc3000.begin(false))
  {
    return false;
  }  
  
  /* Try to use the smart config app (no AES encryption), saving */
  /* the connection details if we succeed */
  if (!cc3000.startSmartConfig(false))
  {
    lcd_print_top("Failed!");
    delay(1000);
    return false;
  }
  lcd_print_top("Connection saved!");
  //do not request DHCP, since we will be resetting the device anyways 
  //"normal" smartconfig create pattern would request DHCP here 
  return true;
}

static void convertByteArrayToAsciiHex(uint8_t* hash, char * returnString, uint8_t byte_array_length) {
  const char digit2Ascii[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  for (int i=0; i<byte_array_length; i++) {
    returnString[2*i    ] = digit2Ascii[hash[i]>>4];
    returnString[2*i + 1] = digit2Ascii[hash[i]&0xf];
  }
  returnString[2*(byte_array_length-1) + 2] = '\0';
}

void _reset(void) {
   time = 5000;
   while (time > 0) {
      lcd_print_top("Resetting in ");
      lcd.setCursor(0,1);
      lcd.print(time/1000); lcd.print(" seconds...");
      time -=1000;
      delay (1000);
   }
   lcd_print_top("Resetting now...");
   tinyWDT.force_reset();
   delay(1000);
}
