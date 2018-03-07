// AudioEffectIntegrator - integrating envelope
// An additional Teensy Audio library effect
// 
// Implements a simple impulse-response-integrator envelope generator.
//
// Mimics the behaviour of envelopes generated by simple R-C-networks 
// as used in analog synthesizers like the TB-303.
//
// noteOn(velocity) triggers the attack but ties the signal to the given velocity (just adds enough energy to keep it)
//
// pulse(velocity) triggers the attack but adds to the existing energy.
//
// The implementation uses two "energy buckets":
// The attack bucket is filled by noteOn() or pulse() and transfers energy to the decay bucket (usually very fast).
// The decay bucket is filled by transfers from the attack bucket and drains energy to a damper (usually slow). 
// The incoming audio is just attenuated according to the decay bucket energy.
//

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
  // set attack time (time until 9/10 of energy is transfered)
	void attack(float milliseconds) {
  	__disable_irq();
    if(milliseconds<=0.f) 
      attack_valve=0.f;
    else    
  		attack_valve = pow(0.1,1.f/(AUDIO_SAMPLE_RATE_EXACT*0.001f*milliseconds));
    __enable_irq();
	}
  // set decay time (time until 9/10 of energy is drained)
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

    float attack_flow=1.-attack_valve;
	  while (p < end) {
      // TODO fast integer implementation.
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


