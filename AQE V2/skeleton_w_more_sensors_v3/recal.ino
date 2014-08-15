
/*
*  Checks EEPROM to see if egg already has calibration data
*/
void initialize_eeprom() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    tinyWDT.pet();
  }
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
    while(Serial.available() <= 0) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        tinyWDT.pet();
      }
    }
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
    while(Serial.available() <= 0) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        tinyWDT.pet();
      }
    }
      if (Serial.available() > 0) {
        CO_M = Serial.parseFloat();
      }
    Serial.println("Enter calibration value for the NO2 sensor: ");
    while(Serial.available() <= 0) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        tinyWDT.pet();
      }
    }
    if (Serial.available() > 0) {
      NO2_M = Serial.parseFloat();
    }
    Serial.println("Enter calibration value for the O3 sensor: ");
    while(Serial.available() <= 0) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        tinyWDT.pet();
      }
    }
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
    while(Serial.available() <= 0) {
    unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        tinyWDT.pet();
      }
    }
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
