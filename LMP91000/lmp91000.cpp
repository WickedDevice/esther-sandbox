#include "LMP91000.h"

LMP91000::LMP91000(uint8_t menb_pin) {
	_menb = menb_pin;

}

//assumes menb is handled correctly
void LMP91000::write(uint8_t reg, uint8_t data) {
	Wire.beginTransmission(LMP91000_I2C_Address_W);
	Wire.write(reg);
	Wire.write(data);
	Wire.endTransmission();
}

uint8_t LMP91000::status(void) {
	Wire.beginTransmission(LMP91000_I2C_Address);
	Wire.write(LMP91000_STATUS_REG);
	if (Wire.endTransmission() != 0) {
		return LMP91000_NOT_PRESENT;
	}
	uint8_t ready = 0;
	Wire.requestFrom(LMP91000_I2C_Address, 1);
	if(Wire.available()) {
		ready = Wire.read();
	}
	return ready &= 1;
}	
	
	//move enable handling to main, out of library
bool LMP91000::begin(uint8_t tiacn, uint8_t refcn, uint8_t modecn) {	
	_tiacn = tiacn;
	_refcn = refcn;
	_modecn = modecn;	
	Wire.begin();
	//check if ready
	//uint8_t ready = 0;
	while (status() != LMP91000_READY) {
		if (status() == LMP91000_NOT_PRESENT) {
			return false;
		}
	}
	
	//set TIA reg
	Wire.beginTransmission(LMP91000_I2C_Address);
	write(LMP91000_LOCK_REG, LMP91000_WRITE_UNLOCK);
	Wire.endTransmission();
	
	Wire.beginTransmission(LMP91000_I2C_Address);
	write(LMP91000_TIACN_REG, _tiacn);
	Wire.endTransmission();
	
	//set REFCN reg
	Wire.beginTransmission(LMP91000_I2C_Address);
	write(LMP91000_REFCN_REG, _refcn);
	Wire.endTransmission();
	
	//set MODECN reg
	Wire.beginTransmission(LMP91000_I2C_Address);
	write(LMP91000_MODECN_REG, _modecn);
	Wire.endTransmission();
	
	Wire.beginTransmission(LMP91000_I2C_Address);
	write(LMP91000_LOCK_REG, LMP91000_WRITE_LOCK);
	Wire.endTransmission();
	
	return true;
}

