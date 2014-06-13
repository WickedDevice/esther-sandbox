/*************************
*  First version of AQE code
*  All sensors are connected (including DHT22), posting to Xively
*  Using external 18-bit differential ADC
*  Prints status messages to LCD screen
*  TODO: Remove all serial monitor related code
*  TODO: Add local data storage option (SD card)
**************************/
#include <WildFire_CC3000.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>
#include <LiquidCrystal.h>
#include <MCP3424.h>
#include <Wire.h>
#include <DHT22.h>
#include <avr/eeprom.h>
#include <TinyWatchdog.h>
#include "MemoryLocations.h"

WildFire wf;
DHT22 myDHT22(26); //A2

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
//#define API_key  "aNMDa3DHcW9XqQUmuFQ7HrbKLqcmVO1dHBWPAQRcng9qHhft"
//#define feedID   "1293806464"
int time = 0;
LiquidCrystal lcd(4, 3, 24, 25, 30, 31);
uint32_t ip;
WildFire_CC3000_Client client;

#define CO_chan 1
#define NO2_chan 2
#define O3_chan 3
MCP3424 MCP(6); // Declaration of MCP3424

double CO_M;
double NO2_M;
double O3_M;
const long magic_number = 0x5485;

char api_key[API_KEY_LENGTH];
char feedID[FEED_ID_LENGTH];
int flag = 0;

TinyWatchdog tinyWDT(14);
unsigned long previousMillis = 0;
long interval = 4000;
int liveness;
#define norm_int 20000
#define smcfg_int 90000

void setup(void)
{  
  wf.begin();
  lcd.begin(16,2);
  uint8_t smc_status = eeprom_read_byte((const uint8_t*)tinyWDT_STATUS);
  if (smc_status == 0x37) {
    tinyWDT.begin(1000, smcfg_int);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0);
    while(!attemptSmartConfigCreate());
    soft_reset();   
  }
  tinyWDT.begin(1000, norm_int);  
  liveness = 0;
  pinMode(sm_button, INPUT_PULLUP);
  
  //Serial.begin(115200);
  lcd_print_top("Push button for");
  lcd_print_bottom("recalibration");
  time = 0;
  while (time < 10) {
    if (!digitalRead(sm_button)) {
      Serial.begin(115200);
      initialize_eeprom();
    }
    else {
      time++;
      tinyWDT.pet();
      lcd.setCursor(15,1);
      lcd.print(10 - time);
      delay(1000);
    }
  }
  lcd.setCursor(15,1);
  lcd.print('\0');
  time = 0;
  //Serial.println(F("Welcome to WildFire!\n")); 
  lcd_print_top("Push button for \n");
  lcd_print_bottom("Smartconfig");
  while (time < 10) {
    //wait for 10 seconds for user to select smartconfig 
    if (!digitalRead(sm_button)) {
      eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0x37);
      soft_reset();
    }
    else {
      //Serial.println(F("Waiting for smartconfig"));
      time++;
      if (time%2 == 0) {
        tinyWDT.pet();
      }
        lcd.setCursor(15,1);
        lcd.print(10 - time);
        delay(1000);
      }
    }
  if (time >= 10) {  
    //Serial.println(F("Attempting to reconnect"));
    lcd_print_top("Reconnecting... ");
    //wdt_reset();
    if(!attemptSmartConfigReconnect()){
      lcd_print_bottom("Failed!");
      //Serial.println(F("Reconnect failed!"));
      cc3000.disconnect();
      delay(1000);
      _reset();
      //soft_reset();
    }
  }
  time = 0;
  
 // while(!displayConnectionDetails());
  
  // Get the website IP & print it
  ip = 0;
  // 173.203.98.29
  ip = 29;
  ip |= (((uint32_t) 98) << 8);
  ip |= (((uint32_t) 203) << 16);
  ip |= (((uint32_t) 173) << 24);
  
  eeprom_read_block(api_key, (const void*)API_KEY_EEPROM_ADDRESS, API_KEY_LENGTH);
  eeprom_read_block(feedID, (const void*)FEED_ID_EEPROM_ADDRESS, FEED_ID_LENGTH); 
  long magic;
  eeprom_read_block(&magic, (const void*)CAL_MAGIC, sizeof(float));
  if(magic != magic_number) {
    //Serial.println("no calibration data!");
    lcd_print_top("Missing sensor");
    lcd_print_bottom("calibration data!");
    for(;;);
    //delay(2000);
      //wdt_reset();
    }
   
  eeprom_read_block(&CO_M, (const void*)CO_cal, sizeof(float));
  eeprom_read_block(&NO2_M, (const void*)NO2_cal, sizeof(float));
  eeprom_read_block(&O3_M, (const void*)O3_cal, sizeof(float));
  
   //Serial.println("The current values are :");
    //Serial.print("CO sensor : "); printDouble(CO_M, 6); Serial.println();
    //Serial.print("NO2 sensor : "); printDouble(NO2_M, 6); Serial.println();
    //Serial.print("O3 sensor : "); printDouble(O3_M, 6); Serial.println();
    //Serial.println();
    
  Serial.begin(115200);
  lcd_print_top("Initializing");
  lcd_print_bottom("sensors... 15:00");
  delay(1000);
  //15 min sensor warmup
  time = 0;
  while (time < 900) { //TODO: change back to 900 seconds
    if (time%2000 == 0) {
      tinyWDT.pet();
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
    Serial.println(time);
    time++;
    delay(1000);
  }
  
  lcd_print_top("Ready to begin");
  lcd_print_bottom("collecting data!");
}

char packet_buffer[2048];
#define BUFSIZE 511

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    tinyWDT.pet();
  }
  client = cc3000.connectTCP(ip, 80);

  //from ADC
  //TODO: check for presence of sensor
  int currTime = millis();
  double CO = getGasConc(CO_chan, CO_M, 2.0); 
  double NO2 = getGasConc(NO2_chan, NO2_M, 0.3);
  double O3 = getGasConc(O3_chan,  O3_M, 0.3);
  double temp;
  double hum;
  DHT22_ERROR_t errorCode = myDHT22.readData();
  char tempbuf_bot[17] = "";
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
      sprintf(tempbuf_bot, "BUS Hung ");
      break;
    }
    case DHT_ERROR_NOT_PRESENT: {
      sprintf(tempbuf_bot, "Not Present ");
      break;
    }
    case DHT_ERROR_ACK_TOO_LONG: {
      sprintf(tempbuf_bot, "ACK time out ");
      break;
    }
    case DHT_ERROR_SYNC_TIMEOUT: {
      sprintf(tempbuf_bot, "Sync Timeout ");
      break;
    }
    case DHT_ERROR_DATA_TIMEOUT: {
      sprintf(tempbuf_bot, "Data Timeout ");
      break;
    }
    case DHT_ERROR_TOOQUICK: {
      sprintf(tempbuf_bot, "Polled too quick ");
      break;
    }
  }
  
  //post
  int datalength = 0;
  char dbuf[10] = "";
  #define DATA_MAX_LENGTH 512
  char data[DATA_MAX_LENGTH] = "\n";
  //char SDdata[DATA_MAX_LENGTH];
  //sprintf(SDdata, itoa(currTime, dbuf, DEC));
  //strcat_P(SDdata, PSTR(","));
  strcat_P(data, PSTR("{\"version\":\"1.0.0\",\"datastreams\" : [{\"id\" : \"CO\",\"current_value\" : \"")); 
  strcat(data,dtostrf(CO, 5, 3, dbuf)); 
  //strcat(SDdata, dbuf);
  //strcat_P(SDdata, PSTR(","));
  strcat_P(data, PSTR("\"},"));
  strcat_P(data, PSTR("{\"id\" : \"NO2\",\"current_value\" : \""));
  strcat(data, dtostrf(NO2, 5, 3, dbuf));
  //strcat(SDdata, dbuf);
  //strcat_P(SDdata, PSTR(","));
  strcat_P(data, PSTR("\"},"));
  strcat_P(data, PSTR("{\"id\" : \"O3\",\"current_value\" : \""));
  strcat(data, dtostrf(O3, 5, 3, dbuf));
  //strcat(SDdata, dbuf);
  //strcat_P(SDdata, PSTR(","));
  if (errorCode == DHT_ERROR_NONE) {
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"Temperature\",\"current_value\" : \""));
    strcat(data, dtostrf(temp, 5, 3, dbuf));
    //strcat(SDdata, dbuf);
    //strcat_P(SDdata, PSTR(","));
    strcat_P(data, PSTR("\"},"));
    strcat_P(data, PSTR("{\"id\" : \"Humidity\",\"current_value\" : \""));
    strcat(data, dtostrf(hum, 5, 3, dbuf));
    //strcat(SDdata, dbuf);
    //strcat_P(SDdata, PSTR(","));
  }
  //strcat_P(SDdata, PSTR("\n"));
  strcat_P(data, PSTR("\"},"));
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
    //Serial.println(F("Connection failed")); 
    client.close();
    delay(1000);   
    _reset();
  }
  //Serial.print(SDdata);
//Cycle through readings? loop1:CO loop2:NO2 loop3:O3 loop4:temp loop5:hum  
switch (flag) {
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
      lcd_print_bottom(tempbuf_bot);
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
      lcd_print_bottom(tempbuf_bot);
    }
    break;
  }
}
flag++;
flag %= 5;
  
    //Serial.println(F("-------------------------------------"));
    //wdt_reset();    
    while (client.connected()) {
      //wdt_reset();      
      while (client.available()) {     
        //wdt_reset();        
        char c = client.read();
        //Serial.print(c);
      }
    }  
    
    //wdt_reset();  
    client.close();
    delay(500);    
}

void lcd_print_top(char* string) {
  lcd.clear();
  lcd.print(string);
}

void lcd_print_bottom(char* string) {
  lcd.setCursor(0,1);
  lcd.print(string);
}

double getGasConc(int channel, double M, double L){
  MCP.Configuration(channel,18,0,1);
  MCP.NewConversion();
  double v = MCP.Measure();
  v /= 1000000.0;
  v /= M;
  //Serial.print("Concentration = ");
  //Serial.print(v);
  //Serial.println(" ppm");
  if (v < L) {
    return 0.00;
  }
  return v;
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
  //Serial.println(1);
  if (!cc3000.begin(false, true))
  {
    //Serial.println(2);
    lcd_print_top("Can’t connect wi");
    lcd_print_bottom("th saved details");
    //wdt_reset();
    //Serial.println(F("Unable to re-connect!? Did you run the SmartConfigCreate"));
    //Serial.println(F("sketch to store your connection details?"));
    return false;
  }
  //Serial.println(3);
  //wdt_reset();
  /* Round of applause! */
  lcd_print_top("Reconnected!");
  //Serial.println(F("Reconnected!"));
  //wdt_reset();
  /* Wait for DHCP to complete */
  lcd_print_bottom("Requesting DHCP");
  //Serial.println(F("\nRequesting DHCP"));
  time = 0;
  while (!cc3000.checkDHCP()) {
    delay(100);
    time += 100;
    if (time%2000 == 0) {
        tinyWDT.pet();
      }
    //wdt_reset();
    if (time > 10000) {
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
  /* Wait for DHCP to complete */
  lcd_print_bottom("Requesting DHCP...");
  //Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) 
  {
    delay(100);
    time += 100;
    if (time%2000 == 0) {
        tinyWDT.pet();
      }
    if (time > 20000) {
      time = 0;
      lcd_print_top("DHCP failed!");
      //Serial.println(F("DHCP failed!"));
      return false;
    }
  }  
  
  return true;
}

void _reset(void) {
   time = 10000;
   while (time > 0) {
      lcd_print_top("Resetting in ");
      lcd.setCursor(0,1);
      lcd.print(time/1000); lcd.print(" seconds...");
      time -=1000;
      delay (1000);
   }
   lcd_print_top("Resetting now...");
   //soft_reset();
}
/*
#define NUM_ANALOG_READ_WRAPPED_SAMPLES 100
// returns the average reading of 100 consecutive samples
uint16_t analogReadWrapped(uint8_t analog_pin){
  uint32_t accumulator = 0;
  uint16_t junk = analogRead(analog_pin);
  
  for(uint8_t ii = 0; ii < NUM_ANALOG_READ_WRAPPED_SAMPLES; ii++){
     accumulator += analogRead(analog_pin);
  }

  return (accumulator / NUM_ANALOG_READ_WRAPPED_SAMPLES);
}
*/
/*
*  Checks EEPROM to see if egg already has calibration data
*/
void initialize_eeprom() {
   long magic;
  eeprom_read_block(&magic, (const void*)CAL_MAGIC, sizeof(float));
  if(magic != magic_number) {
    Serial.println("\nSetting sensor calibration values....");
    _initialize_eeprom();
  }
  else {
    Serial.println("It looks like this egg has already been programmed with calibration data!");
    Serial.println();    
    eeprom_read_block(&CO_M, (const void*)CO_cal, sizeof(float));
    eeprom_read_block(&NO2_M, (const void*)NO2_cal, sizeof(float));
    eeprom_read_block(&O3_M, (const void*)O3_cal, sizeof(float));
    Serial.println("The current values are :");
    Serial.print("CO sensor : "); printDouble(CO_M, 6); Serial.println();
    Serial.print("NO2 sensor : "); printDouble(NO2_M, 6); Serial.println();
    Serial.print("O3 sensor : "); printDouble(O3_M, 6); Serial.println();
    Serial.println();
    Serial.println("Would you like to reset these values? Current data will be overwritten! (y/n)");
    while(Serial.available() <= 0);
    if (Serial.available() > 0) {
      char response = Serial.read();
      if (response == 'y') {
        _initialize_eeprom();
      }
    }   
  }
  Serial.println();
}
int done = 0;
/*
*  Takes user input from serial monitor and writes those values to EEPROM
*/
void _initialize_eeprom() {
  while (!done) {
    Serial.println();
    Serial.println("IMPORTANT: Please include the leading 0 before the decimal point.");
    Serial.println();
    Serial.println("Enter calibration value for the CO sensor: ");
    while(Serial.available() <= 0);
    if (Serial.available() > 0) {
      CO_M = Serial.parseFloat();
    }
    Serial.println("Enter calibration value for the NO2 sensor: ");
      while(Serial.available() <= 0);
    if (Serial.available() > 0) {
      NO2_M = Serial.parseFloat();
    }
      Serial.println("Enter calibration value for the O3 sensor: ");
      while(Serial.available() <= 0);
    if (Serial.available() > 0) {
      O3_M = Serial.parseFloat();
    }
    Serial.print("CO sensor : ");
    printDouble(CO_M, 6);
    Serial.println();
    Serial.print("NO2 sensor : ");
    printDouble(NO2_M, 6);
    Serial.println();
    Serial.print("O3 sensor : ");
    printDouble(O3_M, 6);
    Serial.println();
    Serial.println("Are these values correct? (y/n)");
    while(Serial.available() <= 0);
    char response;
    if (Serial.available() > 0) {
      response = Serial.read();
    }
    if (response == 'y') {
      eeprom_write_block(&CO_M, (void*)CO_cal, sizeof(float));
      eeprom_write_block(&NO2_M, (void*)NO2_cal, sizeof(float));
      eeprom_write_block(&O3_M, (void*)O3_cal, sizeof(float));
      eeprom_write_block(&magic_number, (void*)CAL_MAGIC, sizeof(long));
      eeprom_read_block(&CO_M, (const void*)CO_cal, sizeof(float));
      eeprom_read_block(&NO2_M, (const void*)NO2_cal, sizeof(float));
      eeprom_read_block(&O3_M, (const void*)O3_cal, sizeof(float));
      Serial.println();
      Serial.println("Wrote calibration data :");
      Serial.print("CO sensor : ");
      printDouble(CO_M, 6);
      Serial.println();
      Serial.print("NO2 sensor : ");
      printDouble(NO2_M, 6);
      Serial.println();
      Serial.print("O3 sensor : ");
      printDouble(O3_M, 6);
      Serial.println();
      Serial.println();
      done = 1;
    }
    else {
      done = 0;
    }
  }
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
} 

