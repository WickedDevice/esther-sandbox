/*************************
*  First version of AQE code
*  Only one sensor(CO) is connected, posting to Xively
*  Using onboard ADC
*  Prints status messages to LCD screen
*
*
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
WildFire wf(WILDFIRE_V2);

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

WildFire_CC3000 cc3000(WILDFIRE_V2);
int sm_button = 5;

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Xively parameters
#define WEBSITE  "api.xively.com"
#define API_key  "aNMDa3DHcW9XqQUmuFQ7HrbKLqcmVO1dHBWPAQRcng9qHhft"
#define feedID   "1293806464"
#define FEED_NAME "CO_sensor"


 int time = 0;
LiquidCrystal lcd(3, 2, 14, 15, 20, 21);
uint32_t ip;
WildFire_CC3000_Client client;

#define CO_chan 1
#define NO2_chan 2
#define O3_chan 3
MCP3424 MCP(6); // Declaration of MCP3424

void setup(void)
{  
  wdt_enable(WDTO_8S);
  wf.begin();
  //MCP.Configuration(1,18,0,1); // Channel 1, 18 bits resolution, one-shot mode, amplifier gain = 1
  pinMode(sm_button, INPUT_PULLUP);
  analogReference(INTERNAL2V56);
  lcd.begin(16,2);
  Wire.begin();
  //lcd.setCursor(0,1);
  Serial.begin(115200);

  //Serial.println(F("Welcome to WildFire!\n")); 
  lcd.print("Push button for \n");
  lcd.setCursor(0,1);
  lcd.print("Smartconfig  7");
  while (time < 7000) {
    wdt_reset();
    //wait for 10 seconds for user to select smartconfig 
    if (!digitalRead(sm_button)) {
      while(!attemptSmartConfigCreate()){
        wdt_reset();
        //Serial.println(F("Waiting for Smart Config Create"));
      }
      time = 100000;
      break;
    }
    else {
     // Serial.println(F("Waiting for smartconfig"));
      time += 100;
      delay(100);
      if (!(time % 1000)) {
        lcd_print_top("Push button for \n");
        lcd.setCursor(0,1);
        lcd.print("Smartconfig  "); lcd.print(7 - time/1000); lcd.print("  ");
      }
    }
  }
  if (time >= 7000) {  
    //Serial.println(F("Attempting to reconnect"));
    lcd_print_top("Reconnecting... ");
    wdt_reset();
    if(!attemptSmartConfigReconnect()){
      lcd_print_bottom("Failed!");
      //Serial.println(F("Reconnect failed!"));
      cc3000.disconnect();
      delay(1000);
      _reset();
      soft_reset();
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

   wdt_reset();   
//  cc3000.printIPdotsRev(ip);
  //Serial.println();
  lcd_print_top("Initializing");
  lcd_print_bottom("sensors... 15:00");
  delay(1000);
  //15 min sensor warmup
  int time = 0;
  while (time < 9) {
    wdt_reset();
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
  lcd_print_top("Ready to begin");
  lcd_print_bottom("collecting data!");
  wdt_reset();
}

char packet_buffer[2048];
#define BUFSIZE 511

void loop() {
  wdt_reset();
  //lcd.clear();
  //lcd.print("Connecting....");
  //wdt_disable();
  client = cc3000.connectTCP(ip, 80);
  //lcd.setCursor(0,1);
  //lcd.print("Connected!");
  //wdt_enable(WDTO_8S);
  //Serial.print("1");
  wdt_reset();
  //read
  //directly from sensor:
  //double vref = analogReadWrapped(0)*(2.56 /1023.0);
  //delay(100);
  //double vgas = analogReadWrapped(1)*(2.56 /1023.0);
  //delay(100);

  //from ADC
  //TODO: look this up
  //MCP.Configuration(1,18,0,1);
  double val = getGasConc(CO_chan, 0.000540); //TODO: put this value into EEPROM
  
  //Serial.print("7");
  
  //double val = getGasConc(vgas, vref);
  wdt_reset();
  //post
  int datalength = 0;
  #define DATA_MAX_LENGTH 512
  char data[DATA_MAX_LENGTH] = "\n";
  strcat_P(data, PSTR("{\"version\":\"1.0.0\",\"datastreams\" : [{\"id\" : \"CO_sensorADC\",\"current_value\" : \""));
  char dbuf[10] = "";
  //itoa((int)val, dbuf, 10);
  strcat(data,dtostrf(val, 5, 3, dbuf)); 
  /*
  strcat_P(data, PSTR("\"},"));
  strcat_P(data, PSTR("{\"id\" : \"Vgas\",\"current_value\" : \""));
  strcat(data, dtostrf(vgas, 5, 3, dbuf));
  strcat_P(data, PSTR("\"},"));
  strcat_P(data, PSTR("{\"id\" : \"Vref\",\"current_value\" : \""));
  strcat(data, dtostrf(vref, 5, 3, dbuf));
  */
  strcat_P(data, PSTR("\"}]}"));
  //lcd.print("2");
  datalength = strlen(data);
  /*
  Serial.print("Data length");
  Serial.println(datalength);
  Serial.println();
  */
  wdt_reset();
  char putstr_buffer[64] = "PUT /v2/feeds/";
  strcat_P(putstr_buffer, PSTR(feedID));
  strcat_P(putstr_buffer, PSTR(".json HTTP/1.0"));
  
  char apikey_buffer[128] = "X-ApiKey: ";
  strcat_P(apikey_buffer, PSTR(API_key));
  //lcd.print("3");
  char contentlen_buffer[64] = "Content-Length: ";
  char len_buffer[32] = "";
  itoa(datalength, len_buffer, 10);
  strcat(contentlen_buffer, len_buffer); 

  wdt_reset();    
  if (client.connected()) {
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

    wdt_reset();      
    //lcd.print("6 ");
    
    //lcd.print(strlen(packet_buffer));
    for (int i = 0; i < strlen(packet_buffer); i++) {
        client.print(packet_buffer[i]);
        //Serial.print(packet_buffer[i]);
        wdt_reset();
    }
    //Serial.println();
    client.println();
    
    //client.fastrprintln(packet_buffer);
    //lcd.print("7");    
  } else {
    lcd_print_top("Could not post ");
    lcd_print_bottom("to Xively!");
    //Serial.println(F("Connection failed")); 
    client.close();
    delay(1000);   
    _reset();
  }
  
  lcd_print_top("Current CO value");
  lcd.setCursor(4,1);
  lcd.print(val); lcd.print(" ppm");
  
    //Serial.println(F("-------------------------------------"));
    wdt_reset();    
    while (client.connected()) {
      wdt_reset();      
      while (client.available()) {     
        wdt_reset();        
        char c = client.read();
        Serial.print(c);
      }
    }  
    
    wdt_reset();  
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

double getGasConc(int channel, double M){ //double vgas, double vref) {
  //Voffset = 0?
  //for 101713-46 (0.000446)
  //double v = (vgas - vref)/(.000446);
  //Serial.print("2");
  //wdt_disable();
  MCP.Configuration(channel,18,0,1);
  //Serial.print("3");
  //wdt_reset();
  MCP.NewConversion();
  //Serial.print("4");
  wdt_disable();
  double v = MCP.Measure();
  //Serial.print("5");
  wdt_enable(WDTO_8S);
  wdt_reset();
  v /= 1000000.0;
  v /= M;
  //Serial.print("6");
  if (v < 2) {
    return 0.00;
  }
  return v;
}

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
    wdt_reset();
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
    delay(100); // ToDo: Insert a DHCP timeout!
    time += 100;
    wdt_reset();
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
   soft_reset();
}

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

void printDouble( double val, byte precision){
// prints val with number of decimal places determine by precision
// NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
// example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)
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
