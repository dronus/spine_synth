#include <Audio.h>
#include <math.h> 


// Spine Synth
//
// A somewhat TB-303 like subtractive synthesizer.
//
// Several parameters are choosen to behave more or less then a TB-303.
//
// Some parameters are choosen to perform easier than a TB-303, eg. some envelope timings tied to sequencer speed.
// The sequencer does not follow the complicated step programming scheme, but only allows to enter notes
// in realtime into the running sequence.
//
// Some of the TB-303 unique behaviours are implemented in an easy and simple manner. 
//
// We do not try to match the sound of the original device, but provide the basic qualities 
// of it's basic sound and unique features the most simple emulations lack.
//
// See the TB-303 service manual and
// http://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf
// as reference sources of basic operation and parameters.



AudioSynthWaveformDc     dc1;
AudioEffectEnvelope      env1;
AudioEffectEnvelope      env2;
AudioSynthWaveform       osc1;
AudioSynthWaveform       osc2;
AudioMixer4              mixer1;
AudioFilterStateVariable filter1;
AudioFilterStateVariable filter2;
AudioOutputAnalog        dac1;
AudioOutputUSB           usb2;
AudioConnection          patchCord3(dc1, 0, env1, 0);
AudioConnection          patchCord001(osc1, 0, mixer1, 0);
AudioConnection          patchCord002(osc2, 0, mixer1, 1);
AudioConnection          patchCord0(mixer1, 0, filter1, 0);
AudioConnection          patchCord01(env1, 0, filter1, 1);
AudioConnection          patchCord1(filter1, 0, filter2, 0);
AudioConnection          patchCord11(env1, 0, filter2, 1);
AudioConnection          patchCord2(filter2, 0, env2, 0);
AudioConnection          patchCord5(env2, 0, dac1, 0);
AudioConnection          patchCord6(env2, 0, usb2, 0);
AudioConnection          patchCord7(env2, 0, usb2, 1);

float getDifferentialCapacity(int sendPin, int receivePin1, int receivePin2, int samples, float& total);


void setup(void)
{
  pinMode(22,INPUT_PULLDOWN);
  pinMode(23,INPUT_PULLDOWN);
  pinMode(17,INPUT_PULLUP);
  pinMode(15,INPUT_PULLUP);
  pinMode(14,INPUT_PULLUP);

  pinMode(18,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(13,OUTPUT);

  Serial.begin(9600);

  AudioMemory(64);
  dc1.amplitude(1.0f);
  osc1.begin(WAVEFORM_SAWTOOTH);
  osc1.amplitude(1.0f);
  osc2.begin(WAVEFORM_SQUARE);
  osc2.amplitude(1.0f);
  env1.attack(3.0f); // attack as by TB-303
  env2.attack(3.0f);
  env2.decay(16.0f); // TB-303 has 8ms full on and 8ms linear decay
  Serial.println("spine_synth running.");
}
 
long last_time;
bool digitals_last[5];
bool digitals_click[5];
float last_volume=0.;
int cycle=0;
int base_cycle_length=125;
int cycle_length=base_cycle_length;
int step =0;
float frequencies[16];
bool accents[16];
bool slides[16];

long last_tap=0;

long taps[4];


int scale_notes[]={0,2,4,5,7,9,11};

float note_to_frequency(int note,int scale)
{
  if(scale!=0){
    note+=scale;
    int octave=note/8;
    int key   =note%8;
    note=scale_notes[key]+octave*12;
    note-=scale;
  }
  return 27.5f*exp2(note/12.0f);
}

float accent_integral=0.f;


void loop()
{
  long time;
  long dt=0;
  while(dt<10)
  {
    time=millis();
    dt=time-last_time;
    delay(1);
  }
  last_time = time;

  bool digitals[]={!digitalRead(17),!digitalRead(15),!digitalRead(14),digitalRead(22)};
  for(int i=0;i<4; i++)
  {
    digitals_click[i]=digitals_click[i] || (digitals[i] && !digitals_last[i]);
    digitals_last[i]=digitals[i];
  }

  int analogs[]={0,0,analogRead(A19),analogRead(A18),analogRead(A17),analogRead(A16),analogRead(A15),analogRead(A14),analogRead(A6),analogRead(A7)};

/*  Serial.print(analogs[0]);
  Serial.print(" ");
  Serial.println(analogs[1]);*/


  // tap tempo
  if(digitals_click[2])
  {
    digitals_click[2]=false;
    long four_bars=time-taps[2];
    cycle_length=base_cycle_length=four_bars/12;

    for(int i=2; i>=0; i--)
      taps[i+1]=taps[i];
    taps[0]=time;
  }

  cycle+=dt;

  if(cycle>cycle_length){

    int scale=analogs[9]*12/1024;

    float total=0;
    float diff=getDifferentialCapacity(8, 9, 10, 512, total);

    float volume=total;
    float frequency=-diff/volume+0.25f;
    frequency*=40.0f;
    frequency=note_to_frequency(frequency,scale);

    if(volume<75.f) volume=0.f;

    if(digitals_click[0]) accents[step]=!accents[step];
    digitals_click[0]=false;
    if(digitals_click[1]) slides [step]=!slides [step];
    digitals_click[1]=false;

    float last_frequency=frequencies[step];
    bool  last_slide    =slides[step];
    step++;
    step=step % 16;

    if(volume>0){
      frequencies[step]=frequency;
      accents[step]=digitals[0];
      slides [step]=digitals[1];
    }

    if(digitals[3]) {
      frequencies[step]=0;
      accents[step]=false;
      slides [step]=false;
    }

    accent_integral*=0.7f;
    if(accents[step]) accent_integral+=analogs[6]/1024.f;
    if(accent_integral>1.0f) accent_integral=1.0f;

    // for accented notes, TB-303 decay would be 200ms. We tie that to our cycle, so adjust to 200ms for 120bpm.
    float decay=accents[step] ? cycle_length * 1.6f : analogs[5]/1024.f*4.f*cycle_length;

    AudioNoInterrupts();
    env1.decay(decay);
    env2.sustain(accent_integral*0.5f+0.5f);
    if(frequencies[step]>0 && (!last_slide || last_frequency==0) ) {
      env1.noteOn();
      env2.noteOn();
    }
    if(frequencies[step]==0) {
      env1.noteOff(); 
      env2.noteOff();
    }
    AudioInterrupts();

    digitalWrite(13,step%4==0);
    digitalWrite(18,accents[step]);
    digitalWrite(16,slides [step]);

    cycle-=cycle_length;

    if(step%2==0)  // swing
      cycle_length=base_cycle_length*(1.f-analogs[8]/1024.f*0.5f);
    else
      cycle_length=base_cycle_length*(1.f+analogs[8]/1024.f*0.5f);
  }

  float frequency=frequencies[step];
  if(slides[step]){
    float next_f=frequencies[(step+1)%16];
    float t=((float)cycle)/cycle_length;
    frequency=frequency*(1.f-t)+next_f*t;
  }
  float mix_waveform     =analogs[7]/1024.f;
  float filter_cutoff    =analogs[2]*4.0f;
  float filter_resonance =analogs[3]*5.0f/1024.0f;
  float filter_mod       =(analogs[4]/1024.0f + (accents[step] ? accent_integral : 0.f) )*7.0f;

  AudioNoInterrupts();
  osc1.frequency(frequency);
  osc2.frequency(frequency);
  mixer1.gain(0,    mix_waveform);
  mixer1.gain(1,1.f-mix_waveform);
  filter1.octaveControl(filter_mod);
  filter1.resonance(filter_resonance);
  filter1.frequency(filter_cutoff);
  filter2.octaveControl(filter_mod);
  filter2.resonance(filter_resonance);
  filter2.frequency(filter_cutoff);
  AudioInterrupts();
  /*
  if(millis() - last_time >= 5000) {
    Serial.print("Proc = ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" (");    
    Serial.print(AudioProcessorUsageMax());
    Serial.print("),  Mem = ");
    Serial.print(AudioMemoryUsage());
    Serial.print(" (");    
    Serial.print(AudioMemoryUsageMax());
    Serial.println(")");
    Serial.println(dt);
  }
*/
}
