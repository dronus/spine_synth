// a more simple approach for fast capacitive touch keys
// using only the hardware abstraction definitions from CapacitiveSensor.h
// (tested on Cortex-M4)
//
// The sensor should be updated in a regular manner by calling update(samples)
// and can be read back by get().
//
// get() returns a normalized value using every sample made by update() between calls.
//

#include "CapacitiveSensor.h"

class Capacity
{
public:
  Capacity(){}
  Capacity(int sendPin, int receivePin)
  {
    sBit = PIN_TO_BITMASK(sendPin);
    sReg = PIN_TO_BASEREG(sendPin);
    rBit = PIN_TO_BITMASK(receivePin);
    rReg = PIN_TO_BASEREG(receivePin);
    pinMode(sendPin,OUTPUT);
    digitalWrite(sendPin,LOW);
    pinMode(receivePin,INPUT);
    digitalWrite(receivePin,LOW);
  }
  void update(int samples)
  {
	
    long timeout=1024*samples;
    long value=0;
    for(int i=0; i<samples; i++)
    {
      noInterrupts();
      DIRECT_MODE_OUTPUT(rReg, rBit);
      DIRECT_MODE_INPUT(rReg, rBit);
	    DIRECT_WRITE_HIGH(sReg, sBit);
	    while(!DIRECT_READ(rReg, rBit) && (value < timeout) ) value++;
	    DIRECT_WRITE_LOW(sReg, sBit);
      interrupts();
    }
    value_accum  +=value;
    samples_accum+=samples;

  }
  float get()
  {
    float value=value_accum/(float)samples_accum;
    value_accum=samples_accum=0;
    
    if(baseline==-1.f)
      baseline=value;
    else
      baseline=baseline*0.998f+value*0.002f;

    return value-baseline;
  }
private:
	IO_REG_TYPE sBit;
	volatile IO_REG_TYPE * sReg;
	IO_REG_TYPE rBit;
	volatile IO_REG_TYPE * rReg;
  float baseline=-1.f;
  long value_accum=0,samples_accum=0;
};
