/********************
Basic smartconfig reconnect method
  SmartconfigCreate will only be called with a button press, as it erases previous connection details
  After timeout, SmartConfigReconnect will be called
  If reconnect fails, the WildFire will reset itself
  
  This version uses the external watchdog timer on the V3.
*********************/


#include <WildFire_CC3000.h>
#include <SPI.h>
#include <TinyWatchdog.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>
#include "MemoryLocations.h"

WildFire wf;

WildFire_CC3000 cc3000;
int sm_button = 5;
int time = 0;
uint32_t ip;
WildFire_CC3000_Client client;
#define WEBSITE  "api.xively.com"
TinyWatchdog tinyWDT(14);

unsigned long previousMillis = 0;
int interval = 4000;
#define norm_int 20000
#define smcfg_int 60000

void setup(void)
{  
   wf.begin();
  Serial.begin(115200);
  uint8_t smc_status = eeprom_read_byte((const uint8_t*)tinyWDT_STATUS);
  if (smc_status == 0x37) {
    //Smartconfig was initiated, so attempt to create new connection
    tinyWDT.begin(1000, smcfg_int);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0);
    while(!attemptSmartConfigCreate());
    Serial.println("Connecting...");
    delay(1000);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0x5b);
    tinyWDT.force_reset();
    delay(1000);
  }
  else if (smc_status == 0x5b) {
    //New smartconfig connection was saved, so skip smartconfig timeout
    tinyWDT.begin(1000, norm_int);
    eeprom_write_byte((uint8_t*)tinyWDT_STATUS, 0); 
    pinMode(sm_button, INPUT_PULLUP);
    time = 10;
    Serial.println("Reconnecting...");
  }
  else {
    //Startup as normal
    tinyWDT.begin(1000, norm_int); 
    pinMode(sm_button, INPUT_PULLUP);
    time = 0;
    Serial.println("Push button for Smartconfig");
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
        Serial.print(10 - time); Serial.println(" seconds...");
        delay(1000);
      }
      if (time == 10) {
        Serial.println("Reconnecting...");
      }
    }
  if (time >= 10) {  
    if(!attemptSmartConfigReconnect()){
      Serial.println("Failed!");
      cc3000.disconnect();
      delay(1000);
      _reset();
    }
  }
  time = 0;
  // Get the website IP
  ip = 0;
  while(ip == 0) {
    cc3000.getHostByName(WEBSITE, &ip);
  }
  
  
  Serial.print("Connected to "); 
  cc3000.printIPdotsRev(ip);
  Serial.println();
  
}

void loop() {
  Serial.println("Nothing to do here...");
 while(1){
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    tinyWDT.pet();
  }
 }
}

void _reset(void) {
   time = 10;
   while (time > 0) {
      Serial.print("Resetting in "); Serial.print(time); Serial.println(" seconds...");
      time--;
      delay (1000);
   }
   Serial.println("Resetting now...");
   tinyWDT.force_reset();
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
  //Serial.println(1);
  if (!cc3000.begin(false, true))
  {
    Serial.println(F("Unable to re-connect!? Did you run the SmartConfigCreate"));
    Serial.println(F("sketch to store your connection details?"));
    return false;
  }
  
  /* Round of applause! */
  Serial.println(F("Reconnected!"));

  /* Wait for DHCP to complete */
  Serial.println("Requesting DHCP");
  time = 0;
  while (!cc3000.checkDHCP()) {
    delay(100);
    time += 100;
    if (time%2000 == 0) {
        tinyWDT.pet();
      }
    if (time > 20000) {
      Serial.println("DHCP failed!");
      delay(500);
      return false;
    }
  }  
  return true;
}

boolean attemptSmartConfigCreate(void){
  /* Initialise the module, deleting any existing profile data (which */
  /* is the default behaviour)  */
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin(false))
  {
    return false;
  }  
  
  /* Try to use the smart config app (no AES encryption), saving */
  /* the connection details if we succeed */
  Serial.println(F("Waiting for a SmartConfig connection (~60s) ..."));
  if (!cc3000.startSmartConfig(false))
  {
    Serial.println("Smartconfig failed!");
    delay(1000);
    return false;
  }
  Serial.println(F("Saved connection details and connected to AP!"));
  //do not request DHCP, since we will be resetting the device anyways 
  //"normal" smartconfig create pattern would request DHCP here 
  return true;
}
