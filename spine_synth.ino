#include <Audio.h>
#include <math.h> 


// GUItool: begin automatically generated code
AudioSynthWaveformDc     dc1;
AudioEffectEnvelope      env1;
AudioEffectEnvelope      env2;
AudioEffectMultiply      mult1;
AudioSynthWaveform       osc1;
AudioFilterStateVariable filter1;
AudioFilterStateVariable filter2;
AudioOutputAnalog        dac1;
AudioOutputUSB           usb2;
AudioConnection          patchCord3(dc1, 0, env1, 0);
AudioConnection          patchCord31(dc1, 0, env2, 0);
AudioConnection          patchCord0(osc1, 0, filter1, 0);
AudioConnection          patchCord01(env1, 0, filter1, 1);
AudioConnection          patchCord1(filter1, 0, filter2, 0);
AudioConnection          patchCord11(env1, 0, filter2, 1);
AudioConnection          patchCord2(filter2, 0, mult1, 0);
AudioConnection          patchCord4(env2, 0, mult1, 1);
AudioConnection          patchCord5(mult1, 0, dac1, 0);
AudioConnection          patchCord6(mult1, 0, usb2, 0);
AudioConnection          patchCord7(mult1, 0, usb2, 1);
// GUItool: end automatically generated code

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
  osc1.pulseWidth(0.5);
  osc1.frequency(440.0);
  osc1.amplitude(1.0);
  Serial.println("spine_synth running.");
}
 
long last_time;
bool digitals_last[5];
bool digitals_click[5];
float analogs_slow[]={0.,0.,0.,0.,0.,0.};
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

  bool digitals[]={!digitalRead(17),!digitalRead(15),!digitalRead(14),digitalRead(22),digitalRead(23)};
  for(int i=0;i<5; i++)
  {
    digitals_click[i]=digitals_click[i] || (digitals[i] && !digitals_last[i]);
    digitals_last[i]=digitals[i];
  }

  int analogs[]={analogRead(A5),analogRead(A6),analogRead(A7),c1.capacitiveSensor(128),c2.capacitiveSensor(128)};

  for(int i=0; i<5; i++)
    analogs_slow[i]=analogs_slow[i]*0.8+analogs[i]*0.2;
  
 /*
  for(int i=0; i<5; i++)
  {
    Serial.print(analogs[i]);
    Serial.print(" ");
  }

  for(int i=0; i<2; i++)
  {
    Serial.print(digitals[i]);
    Serial.print(" ");
  }
  Serial.println();
*/
  float volume=analogs_slow[3]+analogs_slow[4];
  float frequency=(analogs_slow[3]-analogs_slow[4])/volume+1.0f;
  frequency*=100.0f;

  volume-=20000.0f;
  if(volume<0.f) volume=0.f;

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

    if(digitals[4]) {
      frequencies[step]=0;
      accents[step]=false;
      slides [step]=false;
    }

    osc1.frequency(frequencies[step]);

    accent_integral*=0.7f;
    if(accents[step]) accent_integral+=0.3f;
    if(accent_integral>1.0f) accent_integral=1.0f;

    env1.attack(cycle_length*0.5f);
    env2.sustain(accent_integral*0.5f+0.5f);
    
    if(frequencies[step]>0 && (!last_slide || last_frequency==0) ) {
      env1.noteOn();
      env2.noteOn();
    }
    if(frequencies[step]==0) {
      env1.noteOff(); 
      env2.noteOff();
    }

    digitalWrite(13,step%4==0);
    digitalWrite(18,accents[step]);
    digitalWrite(16,slides [step]);

    cycle-=cycle_length;
  }


  AudioNoInterrupts();
  if(slides[step]){
    float last_f=frequencies[step];
    float next_f=frequencies[(step+1)%16];
    float t=((float)cycle)/cycle_length;
    float f=last_f*(1.f-t)+next_f*t;
    osc1.frequency(f);
  }

  filter1.octaveControl(analogs_slow[1]/1024.0f*(1.f+accent_integral)*5.0f);
  filter2.octaveControl(analogs_slow[1]/1024.0f*(1.f+accent_integral)*5.0f);
  filter1.resonance(analogs_slow[2]*5.0f/1024.0f);
  filter2.resonance(analogs_slow[2]*5.0f/1024.0f);
  filter1.frequency((1024.f-analogs_slow[0])*4.0f);
  filter2.frequency((1024.f-analogs_slow[0])*4.0f);
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

  }
*/
  Serial.println(dt);
}
