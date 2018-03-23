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

float dbaseline_k=0.1, dbaseline_limit=1.;

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
    //digitalWrite(receivePin,LOW);
  }

  float get(int samples)
  {
	
    long timeout=1024*samples;
    long value_sum=0;

    DIRECT_WRITE_HIGH(sReg, sBit);

    for(int i=0; i<samples; i++)
    {
      noInterrupts();      
      DIRECT_MODE_INPUT(rReg, rBit);
	    while(!DIRECT_READ(rReg, rBit) && (value_sum < timeout) ) value_sum++;
      DIRECT_MODE_OUTPUT(rReg, rBit);
      DIRECT_WRITE_LOW(rReg, rBit);
      interrupts();
    }

    value=value_sum/(float)samples;
            
    if(baseline==-1.f)
      baseline=value;
    else{
      float db=value-baseline;
      db=min(dbaseline_limit,max(-dbaseline_limit,db));
      baseline+=db*dbaseline_k;
    }

    return value-baseline;
  }
private:
	IO_REG_TYPE sBit;
	volatile IO_REG_TYPE * sReg;
	IO_REG_TYPE rBit;
	volatile IO_REG_TYPE * rReg;
  float baseline=-1.f;
  // long value_accum=0,samples_accum=0;
  float value=0.f;
};
