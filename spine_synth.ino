// Spine Synth 303
//
// A somewhat TB-303-like subtractive synthesizer for Teensy 3.x ARM Cortex M4
//
// Provides 16bit audio out via USB sound device and 12bit line-out via the internal DAC.
//
// Several parameters are choosen to behave more or less then a TB-303.
//
// Some parameters are choosen to perform easier than a TB-303, eg. some envelope timings are tied to sequencer speed.
// The sequencer does not follow the complicated step programming scheme, but only allows to enter notes
// in realtime into the running sequence.
//
// Some of the TB-303 unique behaviours are implemented in an most simple manner. 
//
// We do not try to match the sound of the original device, but provide the basic qualities 
// of it's basic sound and unique features the most simple emulations lack.
//
// See the TB-303 service manual and
// http://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf
// as reference sources for basic operation and parameters.
//
// Pinout:
// 13 : Beat indicator on Teensy internal LED 
// 14 : Tap tempo momentary pushbutton to GND 
// 15 : Slide momentary pushbutton to GND 
// 16 : Slide LED and resitor to GND
// 17 : Accent momentary pushbutton to GND 
// 18 : Accent LED and resitor to GND
// 22 : Delete note momentary pushbutton to VCC
// A6 : swing pot to GND and VCC
// A7 : octave pot to GND and VCC
// A14: Overdrive pot to GND and VCC
// A15: ACC Mod pot to GND and VCC
// A16: ENV Decay pot to GND and VCC
// A17: VCF Env Mod pot to GND and VCC
// A18: VCF Resonance pot to GND and VCC
// A19: VCF Cutoff pot to GND and VCC
// A21: Audio out left , over 100uF to jack
// A22: Audio out right, over 100uF to jack



#include <Audio.h>
#include <math.h> 
#include "AudioEffectIntegrator.h"
#include "Capacity.h"

AudioSynthWaveformDc     dc;
AudioEffectIntegrator    vcfEnv;
AudioEffectIntegrator    accEnv;
AudioMixer4              vcfMixer;
AudioSynthWaveform       oscSaw;
AudioFilterStateVariable vcf1;
AudioFilterStateVariable vcf2;
AudioEffectIntegrator    vcaEnv;
AudioMixer4              vcaMixer;
AudioEffectMultiply      vca;
AudioEffectWaveshaper    distort;
AudioOutputAnalog        dac;
AudioOutputUSB           usbOut;
AudioConnection          patchCord0 (dc       , 0, vcfEnv  , 0);
AudioConnection          patchCord3 (oscSaw   , 0, vcf1    , 0);
AudioConnection          patchCord4 (vcfEnv   , 0, vcfMixer, 0);
AudioConnection          patchCord5 (dc       , 0, accEnv  , 0);
AudioConnection          patchCord6 (accEnv   , 0, vcfMixer, 1);
AudioConnection          patchCord7 (vcfMixer , 0, vcf1    , 1);
AudioConnection          patchCord8 (vcf1     , 0, vcf2    , 0);
AudioConnection          patchCord9 (vcfMixer , 0, vcf2    , 1);
AudioConnection          patchCord10(vcf2     , 0, vca     , 0);
AudioConnection          patchCord11(dc       , 0, vcaEnv  , 0);
AudioConnection          patchCord12(vcaEnv   , 0, vcaMixer, 0);
AudioConnection          patchCord13(accEnv   , 0, vcaMixer, 1);
AudioConnection          patchCord14(vcaMixer , 0, vca     , 1);
AudioConnection          patchCord18(vca      , 0, distort , 0);
AudioConnection          patchCord15(distort  , 0, dac     , 0);
AudioConnection          patchCord16(distort  , 0, usbOut  , 0);
AudioConnection          patchCord17(distort  , 0, usbOut  , 1);

Capacity capacities[16];

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
  digitalWrite(13,HIGH);

  Serial.begin(9600);

  AudioMemory(64);
  dc.amplitude(1.0f);
  oscSaw.begin(WAVEFORM_SAWTOOTH);
  oscSaw.amplitude(1.0f);
  vcfEnv.attack(3.0f); // attack as by TB-303, decay is variable
  vcaEnv.attack(3.0f);
  // vcaEnv.decay(3000.f); // "TB-303 VEG has 3s decay" (Devilfish docs)    sounds ugly, why? tones keep on muffled for very long time
  // vcaEnv.decay(16.0f); // "TB-303 has 8ms full on and 8ms linear decay" who said this?
  vcfMixer.gain(0,1.f);
  vcfMixer.gain(1,1.f);
  vcaMixer.gain(0,0.5f);
  vcaMixer.gain(1,0.5f);
  Serial.println("spine_synth running.");

  for(int i=0; i<8; i++)
    capacities[i]=Capacity(0,i+1);    // pins 1 - 8
  for(int i=0; i<8; i++)
    capacities[i+8]=Capacity(0,31-i); // pins 24 - 31 in reverse order

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
long taps[4];

float log_pot(int x)
{
  return 1.f-log2(1024-x)/10.f;
}

float note_to_frequency(int note,int octave)
{
  return 27.5f*exp2(note/12.0f+octave);
}

void loop()
{
  long time;
  long dt=0;
  while(dt<5)
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

  int midiEvent=usbMIDI.read() ? usbMIDI.getType() : 0;
  if(midiEvent){
    Serial.print("MIDI ");Serial.print(midiEvent); Serial.println();
  }

  for(int i=0; i<16; i++)
    capacities[i].update(8);

  if((cycle_length>0 && cycle>cycle_length) || midiEvent==usbMIDI.NoteOn || midiEvent==usbMIDI.NoteOff){

    int octave=analogs[9]*6/1024;

    float frequency=0;
    float volume=0;

    for(int i=0; i<16; i++)
    {    
      float total=capacities[i].get();
      //Serial.print(total); Serial.print(" ");
      if(total>20.f) {
         volume=1;
         frequency=i;
      }      
    }
    //Serial.println();

    frequency=note_to_frequency(frequency,octave);

    if(digitals_click[0]) accents[step]=!accents[step];
    digitals_click[0]=false;
    if(digitals_click[1]) slides [step]=!slides [step];
    digitals_click[1]=false;

    float last_frequency=frequencies[step];
    bool  last_slide    =slides[step];

    int transpose=-21-12*2;

    if(midiEvent==usbMIDI.NoteOn){
      int candidate=note_to_frequency(usbMIDI.getData1()+transpose,octave);
      if(candidate>last_frequency){
        step++;
        step=step % 16;
        frequencies[step]=note_to_frequency(usbMIDI.getData1()+transpose,octave);
        accents[step]=(usbMIDI.getData2()>=100);
        slides [step]=false;
      }
    }else if(midiEvent==usbMIDI.NoteOff){
      if(last_frequency==note_to_frequency(usbMIDI.getData1()+transpose,octave)){
        step++;
        step=step % 16;
        frequencies[step]=0;
        accents[step]=false;
        slides [step]=false;
      }
    }else{
      step++;
      step=step % 16;    
    }

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

    // for accented notes, TB-303 decay would be 200ms. We tie that to our cycle, so adjust to 200ms for 120bpm.
    float decay=accents[step] ? base_cycle_length * 1.6f : log_pot(analogs[5])*8.f*base_cycle_length;
    float accent=analogs[6]/1024.f;
    float accent_slew=base_cycle_length * 1.6f * (1.f+analogs[3]/1024.f);

    AudioNoInterrupts();
    vcfEnv.decay(slides[step] ? 10000.f : decay);
    vcaEnv.decay(slides[step] ? 10000.f : decay * 2.f);
    accEnv.attack(accent_slew*0.2f);  // accent slew tied to 'resonance' pot like TB-303 does.
    accEnv.decay (accent_slew);

    if(frequencies[step]>0 && (!last_slide || last_frequency==0) ) {
      if(accents[step]) accEnv.pulse(accent);
      vcfEnv.noteOn(1.f);
      vcaEnv.noteOn(1.f);
    }

    AudioInterrupts();

    digitalWrite(13,step%4==0);
    digitalWrite(18,accents[step]);
    digitalWrite(16,slides [step]);

    cycle-=cycle_length;

    if(midiEvent)
      cycle_length=0;
    else if(step%2==0)  // swing
      cycle_length=base_cycle_length*(1.f-analogs[8]/1024.f*0.5f);
    else
      cycle_length=base_cycle_length*(1.f+analogs[8]/1024.f*0.5f);
  }

  float frequency=frequencies[step];
  if(slides[step]){
    float next_f=frequencies[(step+1)%16];
    float t=((float)cycle)/base_cycle_length;
    frequency=frequency*(1.f-t)+next_f*t;
  }
  float filter_cutoff    =log_pot(analogs[2])*4096.0f;
  float filter_resonance =analogs[3]*5.0f/1024.0f;
  float filter_mod       =analogs[4]/1024.f;
  float distortion_map[33];
  float distortion       =0.25f+(1.f-analogs[7]/1024.f)*0.75f;
  for(int i=0; i<16; i++){
    float x=1.f-i/16.f;
    float y=powf(abs(x),distortion);
    distortion_map[i   ]=-y;
    distortion_map[32-i]= y;
  }
  distortion_map[16]=0.f;

  AudioNoInterrupts();
  distort.shape(distortion_map,33);
  if(frequency) oscSaw.frequency(frequency);
  vcfMixer.gain(0,filter_mod);
  vcfMixer.gain(1,1.f); // accent is always mixed in, it is just not pulsed any time. 
  vcf1.octaveControl(7.f);
  vcf1.resonance(filter_resonance);
  vcf1.frequency(filter_cutoff);
  vcf2.octaveControl(7.f);
  vcf2.resonance(filter_resonance);
  vcf2.frequency(filter_cutoff);
  AudioInterrupts();
  /*
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
  */
}
