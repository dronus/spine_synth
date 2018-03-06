#include <Audio.h>
#include <math.h> 
#include <MIDI.h>
#include <midi_UsbTransport.h>
#include "AudioEffectIntegrator.h"
#include "Capacity.h"

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
AudioEffectIntegrator    vcfEnv;
AudioEffectIntegrator    accEnv;
AudioMixer4              cvMixer;
AudioSynthWaveform       osc1;
AudioSynthWaveform       osc2;
AudioMixer4              mixer1;
AudioFilterStateVariable filter1;
AudioFilterStateVariable filter2;
AudioEffectIntegrator    vcaEnv;
AudioMixer4              vcaMixer;
AudioEffectMultiply      vca;
AudioFilterStateVariable filter3;
AudioOutputAnalog        dac1;
AudioOutputUSB           usb2;
AudioConnection          patchCord3(dc1, 0, vcfEnv, 0);
AudioConnection          patchCord001(osc1, 0, mixer1, 0);
AudioConnection          patchCord002(osc2, 0, mixer1, 1);
AudioConnection          patchCord0(mixer1, 0, filter1, 0);
AudioConnection          patchCord01(vcfEnv, 0, cvMixer, 0);
AudioConnection          patchCord31(dc1, 0, accEnv, 0);
AudioConnection          patchCord02(accEnv, 0, cvMixer, 1);
AudioConnection          patchCord03(cvMixer, 0, filter1, 1);
AudioConnection          patchCord1(filter1, 0, filter2, 0);
AudioConnection          patchCord11(cvMixer, 0, filter2, 1);
AudioConnection          patchCord2(filter2, 0, vca, 0);
AudioConnection          patchCord32(dc1, 0, vcaEnv, 0);
AudioConnection          patchCord21(vcaEnv, 0, vcaMixer, 0);
AudioConnection          patchCord22(accEnv, 0, vcaMixer, 1);
AudioConnection          patchCord23(vcaMixer, 0, vca, 1);
AudioConnection          patchCord24(vca, 0, filter3, 0); //vca
AudioConnection          patchCord5(vca, 0, dac1, 0);
AudioConnection          patchCord6(vca, 0, usb2, 0);
AudioConnection          patchCord7(vca, 0, usb2, 1);

static const unsigned sUsbTransportBufferSize = 16;
typedef midi::UsbTransport<sUsbTransportBufferSize> UsbTransport;
UsbTransport sUsbTransport;
MIDI_CREATE_INSTANCE(UsbTransport, sUsbTransport, MIDI);

Capacity capacities[8];

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
  dc1.amplitude(1.0f);
  osc1.begin(WAVEFORM_SAWTOOTH);
  osc1.amplitude(1.0f);
  osc2.begin(WAVEFORM_SQUARE);
  osc2.amplitude(1.0f);
  vcfEnv.attack(3.0f); // attack as by TB-303, decay is variable
  vcaEnv.attack(3.0f);
  // vcaEnv.decay(3000.f); // "TB-303 VEG has 3s decay" (Devilfish docs)    sounds ugly, why? tones keep on muffled for very long time
  cvMixer.gain(0,1.f);
  cvMixer.gain(1,1.f);
  vcaMixer.gain(0,0.5f);
  vcaMixer.gain(1,0.5f);
  // vcaEnv.decay(16.0f); // "TB-303 has 8ms full on and 8ms linear decay" who said this?
  filter3.frequency(15000.);
  Serial.println("spine_synth running.");

  for(int i=0; i<8; i++)
    capacities[i]=Capacity(0,i+1);
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

float log_pot(int x)
{
  return 1.f-log2(1024-x)/10.f;
}

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

  for(int i=0; i<8; i++)
    capacities[i].update(8);

  if((cycle_length>0 && cycle>cycle_length) || midiEvent==usbMIDI.NoteOn || midiEvent==usbMIDI.NoteOff){

    int scale=analogs[9]*12/1024;

    float frequency=0;
    float volume=0;

    for(int i=0; i<8; i++)
    {    
      float total=capacities[i].get();
      Serial.print(total);Serial.print(" ");
      if(total>10) {
         volume=1;
         frequency=i+12;
      }      
    }
    Serial.println();

    frequency=note_to_frequency(frequency,scale);

    if(digitals_click[0]) accents[step]=!accents[step];
    digitals_click[0]=false;
    if(digitals_click[1]) slides [step]=!slides [step];
    digitals_click[1]=false;

    float last_frequency=frequencies[step];
    bool  last_slide    =slides[step];

    int transpose=-21+12*(scale-4);

   if(midiEvent==usbMIDI.NoteOn){
      int candidate=note_to_frequency(usbMIDI.getData1()+transpose,0.);
      if(candidate>last_frequency){
        step++;
        step=step % 16;
        frequencies[step]=note_to_frequency(usbMIDI.getData1()+transpose,0.);
        accents[step]=(usbMIDI.getData2()>=100);
        slides [step]=false;
      }
    }else if(midiEvent==usbMIDI.NoteOff){
      if(last_frequency==note_to_frequency(usbMIDI.getData1()+transpose,0.)){
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
/*    if(frequencies[step]==0) {
      vcfEnv.noteOff(); 
      vcaEnv.noteOff();
    }*/
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
  float mix_waveform     =analogs[7]/1024.f;
  float filter_cutoff    =log_pot(analogs[2])*4096.0f;
  float filter_resonance =analogs[3]*5.0f/1024.0f;
  float filter_mod       =analogs[4]/1024.f;

  AudioNoInterrupts();
  if(frequency) osc1.frequency(frequency);
  if(frequency) osc2.frequency(frequency);
  mixer1.gain(0,    mix_waveform);
  mixer1.gain(1,1.f-mix_waveform);
  cvMixer.gain(0,filter_mod);
  cvMixer.gain(1,1.f); // accent is always mixed in, it is just not pulsed any time. 
  filter1.octaveControl(7.f);
  filter1.resonance(filter_resonance);
  filter1.frequency(filter_cutoff);
  filter2.octaveControl(7.f);
  filter2.resonance(filter_resonance);
  filter2.frequency(filter_cutoff);
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
