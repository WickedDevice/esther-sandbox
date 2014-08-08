// -*- c++ -*-
//
// Copyright 2010 Ovidiu Predescu <ovidiu@gmail.com>
// Date: June 2010
// Updated: 08-JAN-2012 for Arduno IDE 1.0 by <Hardcore@hardcoreforensics.com>
//

#include <SPI.h>
#include <WildFire.h>
#include <WildFire_CC3000.h>
#include <WildFire_CC3000_Server.h>
#include <Flash.h>
#include <SD.h>
#include <TinyWebServer.h>

WildFire wf;
WildFire_CC3000 cc3000;

boolean file_handler(TinyWebServer& web_server);
boolean index_handler(TinyWebServer& web_server);

boolean has_filesystem = true;
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Don't forget to modify the IP to an available one on your home network
byte ip[] = { 192, 168, 1, 90 };

const int sdChipSelect = 16;            // SD card chipSelect

#define WLAN_SSID       "myNetwork"        // cannot be longer than 32 characters!
#define WLAN_PASS       "myPassword"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

TinyWebServer::PathHandler handlers[] = {
  {"/", TinyWebServer::GET, &index_handler },
  {"/" "*", TinyWebServer::GET, &file_handler },
  {NULL},
};

boolean file_handler(TinyWebServer& web_server) {
  Serial.println("TWS: file handler");
  char* filename = TinyWebServer::get_file_from_path(web_server.get_path());
  send_file_name(web_server, filename);
  free(filename);
  return true;
}

void send_file_name(TinyWebServer& web_server, const char* filename) {
  Serial.println("TWS: send file name");
  if (!filename) {
    web_server.send_error_code(404);
    web_server.print(F("Could not parse URL"));
  } else {
    TinyWebServer::MimeType mime_type
      = TinyWebServer::get_mime_type_from_filename(filename);
    web_server.send_error_code(200);
    if (file.open(&root, filename, O_READ)) {
	  web_server.send_content_type(mime_type);
	  web_server.end_headers();
      Serial.print(F("Read file ")); Serial.println(filename);
      web_server.send_file(file);
      file.close();
    } else {
	  web_server.send_content_type("text/plain");
	  web_server.end_headers();
	  Serial.print(F("Could not find file: ")); Serial.println(filename);
      web_server.print(F("Could not find file: ")); web_server.println(filename);
    }
  }
}

boolean index_handler(TinyWebServer& web_server) {
  Serial.println("TWS: index handler");
  web_server.send_error_code(200);
  web_server.end_headers();
  web_server.print(F("<html><body><h1>Hello World!</h1></body></html>\n"));
  return true;
}

boolean has_ip_address = false;
TinyWebServer web = TinyWebServer(handlers, NULL);

const char* ip_to_str(const uint8_t* ipAddr)
{
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}

void setup() {
  wf.begin();
  Serial.begin(115200);

  Serial.print(F("Free RAM: ")); Serial.println(FreeRam());
  
  pinMode(9, OUTPUT); // set the SS pin as an output (necessary!)
  digitalWrite(9, HIGH); // but turn off the W5100 chip!
  // initialize the SD card
  
  Serial.print(F("Setting up SD card...\n"));

  if (!card.init(SPI_FULL_SPEED, 16)) {
    Serial.print(F("card failed\n"));
    has_filesystem = false;
  }
  // initialize a FAT volume
  if (!volume.init(&card)) {
    Serial.print(F("vol.init failed!\n"));
    has_filesystem = false;
  }
  if (!root.openRoot(&volume)) {
    Serial.print(F("openRoot failed"));
    has_filesystem = false;
  }

  displayDriverMode();
  
  /* Initialise the module */
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }
  
   char *ssid = WLAN_SSID;             /* Max 32 chars */
  Serial.print(F("\nAttempting to connect to ")); Serial.println(ssid);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode! */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }

  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
  // Start the web server.
  Serial.println(F("Web server starting..."));
  web.begin();

  Serial.println(F("Ready to accept HTTP requests."));
  
}

void loop() {
  web.process();
}

void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER
    Serial.println(F("CC3000 is configure in 'Tiny' mode"));
  #else
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

