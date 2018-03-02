#include <Audio.h>
#include <math.h> 


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

#define FIVE_VOLT_TOLERANCE_WORKAROUND
#include <CapacitiveSensor.h>

CapacitiveSensor c1(8, 9);
CapacitiveSensor c2(8,10);



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

  AudioMemory(30);
  dc1.amplitude(1.0);
  osc1.begin(WAVEFORM_SAWTOOTH);
  osc1.amplitude(1.0);
  osc2.begin(WAVEFORM_SQUARE);
  osc2.amplitude(1.0);
//  osc2.dutyCycle(0.5f);
  Serial.println("spine_synth running.");
}
 
long last_time;
bool digitals_last[5];
bool digitals_click[5];
float last_volume=0.;
int cycle=0;
int cycle_length=150;
int step =0;
float frequencies[16];
bool accents[16];
bool slides[16];

long last_tap=0;

long taps[4];

float quantize(float f)
{
  return exp2(floor(log2(f)*12.0f)/12.0f);
}

float accent_integral=0.f;

void loop()
{

  bool digitals[]={!digitalRead(17),!digitalRead(15),!digitalRead(14),digitalRead(22)};
  for(int i=0;i<4; i++)
  {
    digitals_click[i]=digitals_click[i] || (digitals[i] && !digitals_last[i]);
    digitals_last[i]=digitals[i];
  }

  int analogs[]={c1.capacitiveSensor(128),c2.capacitiveSensor(128),analogRead(A19),analogRead(A18),analogRead(A17),analogRead(A16),analogRead(A15),analogRead(A14),analogRead(A6),analogRead(A7)};

  long time=millis();
  long dt=time-last_time;
  last_time = time;

  // tap tempo
  if(digitals_click[2])
  {
    digitals_click[2]=false;
    long four_bars=time-taps[3];
    cycle_length=four_bars/16;

    for(int i=2; i>=0; i--)
      taps[i+1]=taps[i];
    taps[0]=time;
  }

  cycle+=dt;

  if(cycle>cycle_length){

    float volume=analogs[0]+analogs[1];
    float frequency=(analogs[0]-analogs[1])/volume+1.0f;
    frequency*=100.0f;

    volume-=20000.0f;
    if(volume<0.f) volume=0.f;

    if(digitals_click[0]) accents[step]=!accents[step];
    digitals_click[0]=false;
    if(digitals_click[1]) slides [step]=!slides [step];
    digitals_click[1]=false;

    float last_frequency=frequencies[step];
    bool  last_slide    =slides[step];
    step++;
    step=step % 16;

    if(volume>0){
      frequencies[step]=quantize(frequency);
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

    float decay=accents[step] ? cycle_length * 0.5f : analogs[5]/1024.f*4.f*cycle_length;

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
