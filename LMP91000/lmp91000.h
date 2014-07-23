#ifndef LMP91000_H
#define LMP91000_H

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <Wire.h>

#define LMP91000_I2C_Address	0x48
#define LMP91000_I2C_Address_R	(0x91)                  // Device Address
#define LMP91000_I2C_Address_W	(0x90) 
#define LMP91000_STATUS_REG	(0x00)                  /* Read only status register */
#define LMP91000_LOCK_REG	(0x01)                  /* Protection Register */
#define LMP91000_TIACN_REG	(0x10)                  /* TIA Control Register */
#define LMP91000_REFCN_REG	(0x11)                  /* Reference Control Register*/
#define LMP91000_MODECN_REG	(0x12)                  /* Mode Control Register */

#define LMP91000_WRITE_LOCK	(0x01)
#define LMP91000_WRITE_UNLOCK	(0x00)
#define LMP91000_READY	(0x01)
#define LMP91000_NOT_READY	(0x00)
#define LMP91000_NOT_PRESENT 0xA8

class LMP91000 {

public:
	LMP91000(uint8_t _menb);
	bool begin(uint8_t tiacn, uint8_t refcn, uint8_t modecn);
	void write(uint8_t reg, uint8_t data);
	uint8_t status(void);
	
private:
	uint8_t _menb;
	uint8_t _tiacn;
	uint8_t _refcn;
	uint8_t _modecn;
};

#endif