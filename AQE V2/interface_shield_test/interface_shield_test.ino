/*************************
*  Test for interface shield components
**************************/

#include <SPI.h>
#include <string.h>
#include <stdlib.h>
#include <WildFire.h>
#include <LiquidCrystal.h>
#include <MCP3424.h>
#include <Wire.h>
#include <DHT22.h>

#define DUST_SENSOR 1

WildFire wf;
DHT22 myDHT22(A1);
LiquidCrystal lcd(A3,A2, 4,5,6,8);
MCP3424 MCP(6); //change to appropriate address

#define particulate1 2
#define particulate2 3

int flag = 0;
void initialize_eeprom(void);
void printDouble(double val, byte precision);

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

#if DUST_SENSOR  
  pinMode(particulate1, INPUT);
  pinMode(particulate2, INPUT);
#endif

  MCP.Configuration(1,16,0,1);
}



void loop() {
  //update reading from dust sensor
#if DUST_SENSOR  
  cli();
  double a1 = (double)num_zeros1/ 100.0;
  double a2 = (double)num_zeros2 / 100.0;
  sei();
  updateCounts();
#endif
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
  MCP.NewConversion();
  double v = MCP.Measure();
  v /= 1000000.0; //convert from uV to V
  char data[10];
//Cycle through readings to display on LCD screen
//loop1:CO loop2:NO2 loop3:O3 loop4:temp loop5:hum  
int f = 3;

#if DUST_SENSOR
f = 5;
#endif

switch (flag % f) {
  case 0: {
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
  case 1: {
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
  case 2: {
    lcd_print_top("    Voltage: ");
    lcd_print_bottom(dtostrf(v, 5, 3, data)); lcd.print(" V");
    break;
  }
#if DUST_SENSOR  
  case 3: {
    lcd_print_top("Small particles");
    lcd.setCursor(5,1);
    lcd.print(a1); lcd.print(" %");
    break;
  }
  case 4: {
    lcd_print_top("Large particles");
    lcd.setCursor(5,1);
    lcd.print(a2); lcd.print(" %");
    break;
  }
#endif 
}
flag++;

    delay(2000);    
}

void lcd_print_top(char* string) {
  lcd.clear();
  lcd.print(string);
}

void lcd_print_bottom(char* string) {
  lcd.setCursor(0,1);
  lcd.print(string);
}


