// #include "Arduino.h"
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
    pinMode(receivePin,INPUT);
  }
  void update(int samples)
  {
	  DIRECT_MODE_OUTPUT(sReg, sBit);
	  DIRECT_WRITE_LOW(sReg, sBit);

    long timeout=1024*samples;
    long value=0;
    for(int i=0; i<samples; i++)
    {
      // make sure read pins are discharged
	    DIRECT_MODE_OUTPUT(rReg, rBit);
	    DIRECT_MODE_INPUT(rReg, rBit);

      noInterrupts();
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
    
    baseline++;
    if(baseline>value) baseline=value;

    return value-baseline;
  }
private:
	IO_REG_TYPE sBit;
	volatile IO_REG_TYPE * sReg;
	IO_REG_TYPE rBit;
	volatile IO_REG_TYPE * rReg;
  float baseline=1024.f;
  long value_accum=0,samples_accum=0;
};
