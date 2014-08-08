// -*- c++ -*-
//
// Copyright 2010 Ovidiu Predescu <ovidiu@gmail.com>
// Date: December 2010
// Updated: 08-JAN-2012 for Arduno IDE 1.0 by <Hardcore@hardcoreforensics.com>
//

#include <pins_arduino.h>
#include <SPI.h>
#include <WildFire.h>
#include <WildFire_CC3000.h>
#include <WildFire_CC3000_Server.h>
#include <Flash.h>
#include <SD.h>
#include <TinyWebServer.h>

WildFire wf;
WildFire_CC3000 cc3000;
/****************VALUES YOU CHANGE*************/
// The LED attached to PIN X on an Arduino board.
const int LEDPIN = 6;

// pin 4 is the SPI select pin for the SDcard
const int SD_CS = 16;

// pin 10 is the SPI select pin for the Ethernet
const int CC3000_CS = 21;

// Don't forget to modify the IP to an available one on your home network
byte ip[] = { 192, 168, 5, 177 };
/*********************************************/
#define WLAN_SSID       "myNetwork"        // cannot be longer than 32 characters!
#define WLAN_PASS       "myPassword"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

//static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// The initial state of the LED
int ledState = LOW;

void setLedEnabled(boolean state) {
  ledState = state;
  digitalWrite(LEDPIN, ledState);
}

inline boolean getLedState() { return ledState; }

boolean file_handler(TinyWebServer& web_server);
boolean blink_led_handler(TinyWebServer& web_server);
boolean led_status_handler(TinyWebServer& web_server);
boolean index_handler(TinyWebServer& web_server);

TinyWebServer::PathHandler handlers[] = {
  // Work around Arduino's IDE preprocessor bug in handling /* inside
  // strings.
  //
  // `put_handler' is defined in TinyWebServer
  {"/", TinyWebServer::GET, &index_handler },
  {"/upload/" "*", TinyWebServer::PUT, &TinyWebPutHandler::put_handler },
  {"/blinkled", TinyWebServer::POST, &blink_led_handler },
  {"/ledstatus" "*", TinyWebServer::GET, &led_status_handler },
  {"/" "*", TinyWebServer::GET, &file_handler },
  {NULL},
};

const char* headers[] = {
  "Content-Length",
  NULL
};

TinyWebServer web = TinyWebServer(handlers, headers);

boolean has_filesystem = true;
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

void send_file_name(TinyWebServer& web_server, const char* filename) {
  Serial.println("Send file name.");
  if (!filename) {
    web_server.send_error_code(404);
    web_server.print(F("Could not parse URL"));
  } else {
    TinyWebServer::MimeType mime_type
      = TinyWebServer::get_mime_type_from_filename(filename);
    web_server.send_error_code(200);
    web_server.send_content_type(mime_type);
    web_server.end_headers();
    if (file.open(&root, filename, O_READ)) {
      Serial.print(F("Read file ")); Serial.println(filename);
      web_server.send_file(file);
      file.close();
    } else {
      web_server.print(F("Could not find file: ")); web_server.println(filename);
    }
  }
}

boolean file_handler(TinyWebServer& web_server) {
  Serial.println("File handler.");
  char* filename = TinyWebServer::get_file_from_path(web_server.get_path());
  send_file_name(web_server, filename);
  free(filename);
  return true;
}

boolean blink_led_handler(TinyWebServer& web_server) {
  Serial.println("Blink LED handler.");
  web_server.send_error_code(200);
  web_server.send_content_type("text/plain");
  web_server.end_headers();
  // Reverse the state of the LED.
  setLedEnabled(!getLedState());
  WildFire_CC3000_ClientRef client = web_server.get_client();
  if (client.available()) {
    char ch = (char)client.read();
    if (ch == '0') {
      setLedEnabled(false);
    } else if (ch == '1') {
      setLedEnabled(true);
    }
  }
  return true;
}

boolean led_status_handler(TinyWebServer& web_server) {
  Serial.println("LED status handler.");
  web_server.send_error_code(200);
  web_server.send_content_type("text/plain");
  web_server.end_headers();
  WildFire_CC3000_ClientRef client = web_server.get_client();
  client.println(getLedState(), DEC);
  return true;
}

boolean index_handler(TinyWebServer& web_server) {
  Serial.println("Index handler.");
  send_file_name(web_server, "INDEX.HTM");
  return true;
}

void file_uploader_handler(TinyWebServer& web_server,
			   TinyWebPutHandler::PutAction action,
			   char* buffer, int size) {
  Serial.println("File uploader handler.");
  static uint32_t start_time;
  static uint32_t total_size;

  switch (action) {
  case TinyWebPutHandler::START:
    start_time = millis();
    total_size = 0;
    if (!file.isOpen()) {
      // File is not opened, create it. First obtain the desired name
      // from the request path.
      char* fname = web_server.get_file_from_path(web_server.get_path());
      if (fname) {
	Serial.print(F("Creating ")); Serial.println(fname);
	file.open(&root, fname, O_CREAT | O_WRITE | O_TRUNC);
	free(fname);
      }
    }
    break;

  case TinyWebPutHandler::WRITE:
    if (file.isOpen()) {
      file.write(buffer, size);
      total_size += size;
    }
    break;

  case TinyWebPutHandler::END:
    file.sync();
    Serial.print(F("Wrote ")); Serial.print(file.fileSize()); Serial.print(F(" bytes in "));
    Serial.print(millis() - start_time); Serial.print(F(" millis (received "));
    Serial.print(total_size); Serial.println(F(" bytes)"));
    file.close();
  }
}

void setup() {
  wf.begin();
  Serial.begin(115200);
  Serial.print(F("Free RAM: ")); Serial.println(FreeRam());

  pinMode(LEDPIN, OUTPUT);
  setLedEnabled(false);

  pinMode(SS_PIN, OUTPUT);	// set the SS pin as an output
                                // (necessary to keep the board as
                                // master and not SPI slave)
  digitalWrite(SS_PIN, HIGH);	// and ensure SS is high

  // Ensure we are in a consistent state after power-up or a reset
  // button These pins are standard for the Arduino w5100 Rev 3
  // ethernet board They may need to be re-jigged for different boards
  pinMode(CC3000_CS, OUTPUT); 	// Set the CS pin as an output
  digitalWrite(CC3000_CS, HIGH); // Turn off the W5100 chip! (wait for
                                // configuration)
  pinMode(SD_CS, OUTPUT);       // Set the SDcard CS pin as an output
  digitalWrite(SD_CS, HIGH); 	// Turn off the SD card! (wait for
                                // configuration)

  // initialize the SD card.
  Serial.println(F("Setting up SD card..."));
  // Pass over the speed and Chip select for the SD card
  if (!card.init(SPI_FULL_SPEED, SD_CS)) {
    Serial.println(F("card failed"));
    has_filesystem = false;
  }
  // initialize a FAT volume.
  if (!volume.init(&card)) {
    Serial.println(F("vol.init failed!"));
    has_filesystem = false;
  }
  if (!root.openRoot(&volume)) {
    Serial.println(F("openRoot failed"));
    has_filesystem = false;
  }

  if (has_filesystem) {
    // Assign our function to `upload_handler_fn'.
    TinyWebPutHandler::put_handler_fn = file_uploader_handler;
  }

  // Initialize the Ethernet.
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
  if (has_filesystem) {
    web.process();
  }
}

/**************************************************************************/
/*!
    @brief  Displays the driver mode (tiny of normal), and the buffer
            size if tiny mode is not being used

    @note   The buffer size and driver mode are defined in cc3000_common.h
*/
/**************************************************************************/
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
