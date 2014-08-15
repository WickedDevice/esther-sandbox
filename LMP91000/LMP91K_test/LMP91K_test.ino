#include <WildFire.h>
#include <Wire.h>
#include <lmp91000.h>

#define TIACN_REG_VAL 0x1c
#define CO_REFCN 0x91
#define NO2_REFCN 0xd5
#define O3_REFCN 0xd1
#define MODECN_REG_VAL 0x03
#define CO_MENB 7

WildFire wf;
LMP91000 CO_LMP(CO_MENB);

void setup() {
  wf.begin();
  Serial.begin(115200);
  pinMode(CO_MENB, OUTPUT);
  digitalWrite(CO_MENB, HIGH);
  
}

void loop() {
  //configure LMP91K as expected
  Serial.println("Test 1: Configuring LMP91000 for CO ");
  Serial.print("TIACN register: "); Serial.println(TIACN_REG_VAL);
  Serial.print("CO_REFCN register: "); Serial.println(CO_REFCN);
  Serial.print("MODECN register: "); Serial.println(MODECN_REG_VAL);
  
  //digitalWrite(CO_MENB, HIGH);
  CO_LMP.begin(TIACN_REG_VAL, CO_REFCN, MODECN_REG_VAL);
  //read back configured registers
  uint8_t res = 0;
  Serial.println("After config..");
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_TIACN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("TIACN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_REFCN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("REFCN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_MODECN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("MODECN register: "); Serial.println(res);
  
  Serial.println("Test 2: Configuring LMP91000 for NO2");
  //digitalWrite(CO_MENB, HIGH);
  CO_LMP.begin(TIACN_REG_VAL, NO2_REFCN, MODECN_REG_VAL);
  //read back configured registers
  res = 0;
  Serial.println("After config..");
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_TIACN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("TIACN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_REFCN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("REFCN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_MODECN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("MODECN register: "); Serial.println(res);
  
  Serial.println("Test 3: Configuring the LMP91000 for O3");
  CO_LMP.begin(TIACN_REG_VAL, O3_REFCN, MODECN_REG_VAL);
  //read back configured registers
  res = 0;
  Serial.println("After config..");
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_TIACN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("TIACN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_REFCN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("REFCN register: "); Serial.println(res);
  Wire.beginTransmission(LMP91000_I2C_Address);
  Wire.write(LMP91000_MODECN_REG);
  Wire.requestFrom(LMP91000_I2C_Address, 1);
  if(Wire.available()) {
    res = Wire.read();
  }
  Serial.print("MODECN register: "); Serial.println(res);
  
  //hw tests
  //1. no module present
  Serial.println("Test 4: No module present");
  if (CO_LMP.begin(TIACN_REG_VAL, CO_REFCN, MODECN_REG_VAL)) {
      Serial.println("Test failed!");
  }
  else {
      Serial.println("Test passed!");
  }
  
  /*TODO: devise test for broken sensor and get baseline values*/
}
