/********************
Basic smartconfig reconnect method
  SmartconfigCreate will only be called with a button press, as it erases previous connection details
  After timeout, SmartConfigReconnect will be called
  If reconnect fails, the WildFire will reset itself
*********************/


#include <WildFire_CC3000.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>

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
int time = 0;
uint32_t ip;
WildFire_CC3000_Client client;

void setup(void)
{  
  wdt_enable(WDTO_8S);
  wf.begin();
  pinMode(sm_button, INPUT_PULLUP);
  Serial.begin(115200);

  Serial.println(F("Welcome to WildFire!\n")); 
  Serial.println("Push button for SmartConfig Create");
  while (time < 10000) {
    wdt_reset();
    //wait for 10 seconds for user to select smartconfig 
    
    if (!digitalRead(sm_button)) {
      while(!attemptSmartConfigCreate()){
        wdt_reset();
        Serial.println(F("Waiting for Smart Config Create"));
      }
      time = 100000;
      break;
    }
    else {
      //Serial.println(F("Waiting for smartconfig"));
      time += 100;
      delay(100);
      if (!(time % 1000)) {
        Serial.print(11 - time/1000); Serial.println(" seconds left...");
      }
    }
  }
  if (time >= 10000) {  
    Serial.println(F("Attempting to reconnect"));
    wdt_reset();
    if(!attemptSmartConfigReconnect()){
      Serial.println(F("Reconnect failed!"));
      cc3000.disconnect();
      delay(1000);
      _reset();
    }
  }
  time = 0;
  
  while(!displayConnectionDetails());
  
  // Get the website IP & print it
  ip = 0;
  // 173.203.98.29 (Xively static ip)
  ip = 29;
  ip |= (((uint32_t) 98) << 8);
  ip |= (((uint32_t) 203) << 16);
  ip |= (((uint32_t) 173) << 24);
  
  Serial.print("Connected to "); 
  cc3000.printIPdotsRev(ip);
  Serial.println();
  
}

void loop() {
  Serial.println("Nothing to do here...");
 while(1){
   wdt_reset();
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
   soft_reset();
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
    wdt_reset();
    Serial.println(F("Unable to re-connect!? Did you run the SmartConfigCreate"));
    Serial.println(F("sketch to store your connection details?"));
    return false;
  }
  wdt_reset();
  /* Round of applause! */
  Serial.println(F("Reconnected!"));
  wdt_reset();
  /* Wait for DHCP to complete */
  Serial.println(F("\nRequesting DHCP"));
  time = 0;
  while (!cc3000.checkDHCP()) {
    delay(100);
    time += 100;
    wdt_reset();
    if (time > 10000) {
      Serial.println(F("DHCP failed!"));
      return false;
    }
  }  
  return true;
}

boolean attemptSmartConfigCreate(void){
  /* Initialise the module, deleting any existing profile data (which */
  /* is the default behaviour)  */
  wdt_disable();
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin(false))
  {
    return false;
  }  
  
  /* Try to use the smart config app (no AES encryption), saving */
  /* the connection details if we succeed */
  wdt_enable(WDTO_8S);
  Serial.println(F("Waiting for a SmartConfig connection (~60s) ..."));
  if (!cc3000.startSmartConfig(false))
  {
    delay(1000);
    Serial.println(F("SmartConfig failed"));
    return false;
  }
  wdt_reset();
  Serial.println(F("Saved connection details and connected to AP!"));
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) 
  {
    delay(100);
    time += 100;
    wdt_reset();
    if (time > 20000) {
      time = 0;
      Serial.println(F("DHCP failed!"));
      return false;
    }
  }  
  
  return true;
}
