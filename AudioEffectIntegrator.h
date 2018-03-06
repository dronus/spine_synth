
class AudioEffectIntegrator : public AudioStream
{
public:
	AudioEffectIntegrator() : AudioStream(1, inputQueueArray) {
		attack(0.01f);
		decay(1.f);
	}
	using AudioStream::release;
  void noteOn(float velocity)
  {
  	__disable_irq();
    energy_in=velocity-energy;
    __enable_irq();
  }
  void pulse(float velocity)
  {
  	__disable_irq();
    energy_in+=velocity;
    if(energy+energy_in>1.f) energy_in=1.f-energy;
    __enable_irq();
  }
	void attack(float milliseconds) {
  	__disable_irq();
    if(milliseconds<=0.f) 
      attack_valve=0.f;
    else    
  		attack_valve = pow(0.1,1.f/(AUDIO_SAMPLE_RATE_EXACT*0.001f*milliseconds));
    __enable_irq();
	}
	void decay(float milliseconds) {
  	__disable_irq();
    if(milliseconds<=0.f)
      decay_valve=0.f;
    else
  		decay_valve = pow(0.1,1.f/(AUDIO_SAMPLE_RATE_EXACT*0.001f*milliseconds));
    __enable_irq();
	}
	virtual void update(void)	{
	  int16_t *p, *end;

	  audio_block_t* block = receiveWritable();
	  if (!block) return;

	  p = (int16_t*)(block->data);
	  end = p + AUDIO_BLOCK_SAMPLES;

/*
    Serial.print(attack_valve); Serial.print(" ");
    Serial.print(decay_valve); Serial.print(" ");
    Serial.print(attack_valve); Serial.print(" ");
    Serial.print(energy_in); Serial.print(" ");
    Serial.println(energy);Serial.println();
*/

    float attack_flow=1.-attack_valve;
	  while (p < end) {
      float de=energy_in*attack_flow;
      energy    = energy*decay_valve + de;
      energy_in = energy_in          - de;
      (*p++)*=energy;
	  }
	  transmit(block);
	  release(block);
  }
private:
	audio_block_t *inputQueueArray[1];
  float energy_in=0.f,energy=0.f;
  float attack_valve, decay_valve;
};


