// #include "Arduino.h"
#include "CapacitiveSensor.h"

float getDifferentialCapacity(int sendPin, int receivePin1, int receivePin2, int samples, float& total)
{
	IO_REG_TYPE sBit = PIN_TO_BITMASK(sendPin);
	volatile IO_REG_TYPE * sReg = PIN_TO_BASEREG(sendPin);
	IO_REG_TYPE r1Bit = PIN_TO_BITMASK(receivePin1);
	volatile IO_REG_TYPE * r1Reg = PIN_TO_BASEREG(receivePin1);
	IO_REG_TYPE r2Bit = PIN_TO_BITMASK(receivePin2);
	volatile IO_REG_TYPE * r2Reg = PIN_TO_BASEREG(receivePin2);

	DIRECT_MODE_OUTPUT(sReg, sBit);
	DIRECT_WRITE_LOW(sReg, sBit);

  pinMode(sendPin,OUTPUT);
  pinMode(receivePin1,INPUT);
  pinMode(receivePin2,INPUT);

  long timeout=1024*samples;
  long both=0,p1=0,p2=0;
  for(int i=0; i<samples; i++)
  {
    // make sure read pins are discharged
	  DIRECT_MODE_OUTPUT(r1Reg, r1Bit);
	  DIRECT_MODE_OUTPUT(r2Reg, r2Bit);
//	  delayMicroseconds(10);
	  DIRECT_MODE_INPUT(r1Reg, r1Bit);
	  DIRECT_MODE_INPUT(r2Reg, r2Bit);

    noInterrupts();
	  DIRECT_WRITE_HIGH(sReg, sBit);
	  while(!DIRECT_READ(r1Reg, r1Bit) && !DIRECT_READ(r2Reg, r2Bit) && (total < timeout) ) both++;
    while(!DIRECT_READ(r1Reg, r1Bit) && p1<timeout) p1++;
    while(!DIRECT_READ(r2Reg, r2Bit) && p2<timeout) p2++;
	  DIRECT_WRITE_LOW(sReg, sBit);
/*	  while(DIRECT_READ(r1Reg, r1Bit) && DIRECT_READ(r2Reg, r2Bit) && (total < timeout) ) both++;
    while(DIRECT_READ(r1Reg, r1Bit) && p1<timeout) p1++;
    while(DIRECT_READ(r2Reg, r2Bit) && p2<timeout) p2++;*/
    interrupts();
  }
  
  total=(p1+p2+both)/(float)samples;

  return (p2-p1)/(float)samples;
}
