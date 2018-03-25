// Spine Synth 303
//
// A somewhat TB-303-like subtractive synthesizer for Teensy 3.x ARM Cortex M4
//
// Simple UI made of several pots, pushbuttons, LEDs and 16 touch sensitive keys (anything conductive will do).
// 
// Provides 16bit audio out via USB sound device and 12bit line-out via the internal DAC.
// 
// Provides MIDI-in via USB
//
// Several parameters are choosen to behave more or less then a TB-303.
//
// Some parameters are choosen to perform easier than a TB-303, eg. some envelope timings are tied to sequencer speed.
// The sequencer does not follow the complicated step programming scheme, but only allows to enter notes
// in realtime into the running sequence.
//
// Some of the TB-303 unique behaviours are implemented in a most simple manner. 
//
// We do not try to match the sound of the original device, but provide the basic qualities 
// of it's sound and unique features the most simple emulations lack.
//
// See the TB-303 service manual and
// http://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf
// as reference sources for basic operation and parameters.
//
// Hardware needed:
//
// 1x Teensy 3.5 or 3.6
// 4x momentary pushbuttons, 3 of them with LEDs or additional LEDs with resistors.
// 8x Variable resistors, 50kOhm linear, preferrably robust and creamy ones.
// 16x touch key, can be just a piece of copper foil or pcb.
// 1x stereo audio socket (3.5mm, 6.3mm line or dual RCA)
// 2x 100uF 5v min. electrolytic capacitor
//
// Pinout:
//  0 : Send pin for capacitive touch keyboard. Connect to all keys via 1M resistors
// 1-8: Lower keyboard keys. Connect to 8 keys.
// 9-12: Octave selector 10 position 4 bit switch to VCC.
// 24-31: Upper keyboard keys. Connect to another 8 keys in reverse order.
// 13 : Beat indicator on tap tempo button LED and resistor to GND, also Teensy internal LED
// 14 : Tap tempo momentary pushbutton to GND 
// 15 : Slide momentary pushbutton to GND 
// 16 : Slide LED and resistor to GND
// 17 : Accent momentary pushbutton to GND 
// 18 : Accent LED and resistor to GND
// 22 : Delete note momentary pushbutton to VCC
// 21 : MIDI-in, via optocoupler from DIN socket
// A19: OSC MIX to GND and VCC
// A18: CUTOFF FREQ pot to GND and VCC
// A17: RESONANCE pot to GND and VCC
// A16: ENV MOD pot to GND and VCC
// A15: DECAY pot to GND and VCC
// A14: ACCENT pot to GND and VCC
// A5 : ACCENT DECAY pot to GND and VCC
// A6 : SWING% pot to GND and VCC
// A21: Audio out left , over 220 ohms, volume pot left, volume pot slider, 100uF  to jack
// A22: Audio out right, over 220 ohms, volume pot right, 100uF to jack

#include <math.h>
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>
#include "AudioEffectIntegrator.h"
#include "filter_variable_f32.h"
#include "Capacity.h"
#include <MIDI.h>

// configure Teensy Audio library node network
AudioEffectIntegrator        vcfEnv;
AudioEffectIntegrator        accEnv;
AudioMixer4_F32              vcfMixer;
AudioSynthWaveform_F32       oscSaw;
AudioSynthWaveform_F32       oscSquare;
AudioMixer4_F32              oscMixer;
AudioFilterStateVariable_F32 vcf1;
AudioFilterStateVariable_F32 vcf2;
AudioEffectIntegrator        vcaEnv;
AudioMixer4_F32              vcaMixer;
AudioMultiply_F32            vca;
AudioFilterStateVariable_F32 vcaFilter;
AudioConvert_F32toI16        float2Int;
AudioMixer4                  invMixer;
AudioOutputAnalogStereo      dacs;
AudioOutputUSB               usbOut;

AudioConnection_F32          patchCord0 (oscSaw   , 0, oscMixer , 0);
AudioConnection_F32          patchCord1 (oscSquare, 0, oscMixer , 1);
AudioConnection_F32          patchCord2 (oscMixer , 0, vcf1     , 0);
AudioConnection_F32          patchCord3 (vcfEnv   , 0, vcfMixer , 0);
AudioConnection_F32          patchCord4 (accEnv   , 0, vcfMixer , 1);
AudioConnection_F32          patchCord5 (vcfMixer , 0, vcf1     , 1);
AudioConnection_F32          patchCord6 (vcf1     , 0, vcf2     , 0);
AudioConnection_F32          patchCord7 (vcfMixer , 0, vcf2     , 1);
AudioConnection_F32          patchCord8 (vcf2     , 0, vca      , 0);
AudioConnection_F32          patchCord9 (vcaEnv   , 0, vcaMixer , 0);
AudioConnection_F32          patchCord10(accEnv   , 0, vcaMixer , 1);
AudioConnection_F32          patchCord11(vcaMixer , 0, vca      , 1);
AudioConnection_F32          patchCord12(vca      , 0, vcaFilter ,0);
AudioConnection_F32          patchCord13(vcaFilter, 0, float2Int ,0);
AudioConnection              patchCord14(float2Int, 0, invMixer , 0);
AudioConnection              patchCord15(float2Int, 0, dacs     , 0);
AudioConnection              patchCord16(invMixer , 0, dacs     , 1);
AudioConnection              patchCord17(float2Int, 0, usbOut   , 0);
AudioConnection              patchCord18(float2Int, 0, usbOut   , 1);

// MIDI interface
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// capacitive touch keys
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

  pinMode(9,INPUT_PULLDOWN);
  pinMode(10,INPUT_PULLDOWN);
  pinMode(11,INPUT_PULLDOWN);
  pinMode(12,INPUT_PULLDOWN);

  Serial.begin(9600);

  AudioMemory(32);
  AudioMemory_F32(64);
  oscSaw.begin(WAVEFORM_SAWTOOTH);
  oscSaw.amplitude(1.f);
  oscSquare.begin(WAVEFORM_SQUARE);
  oscSquare.amplitude(1.f);  
  vcfEnv.attack(3.0f); // attack as by TB-303, decay is variable
  vcfMixer.gain(1,1.f); // accent is always mixed in, it is just not pulsed any time. 
  vcf1.octaveControl(7.f);
  vcf2.octaveControl(7.f);
  vcaEnv.attack(3.0f);
  vcaEnv.decay(3000.f); // "TB-303 VEG has 3s decay" (Devilfish docs)    sounds ugly, why? tones keep on muffled for very long time
  vcaMixer.gain(0,0.5f);
  vcaMixer.gain(1,0.5f);
  vcaFilter.frequency(5000.f);  
  invMixer.gain(0,-1.f);
  dacs.analogReference(INTERNAL);

  for(int i=0; i<8; i++)
    capacities[i]=Capacity(0,i+1);    // pins 1 - 8
  for(int i=0; i<8; i++)
    capacities[i+8]=Capacity(0,31-i); // pins 24 - 31 in reverse order

  Serial1.setRX(21); // MIDI in pin 21
  MIDI.begin(1); // listening on channel 1
  pinMode(1,INPUT); // reclaim touch input on pin 1 from MIDI (we only use MIDI-in on pin 21)

  // settle capacitive baseline adjustment
  for(int i=0; i<16; i++)
    capacities[i].get(32);

  Serial.println("spine_synth running.");
}
 
long last_time;
bool digitals_last[5];
bool digitals_click[5];
int cycle=0;
int base_cycle_length=125;
int cycle_length=base_cycle_length;
int step =0;
#define STEPS 32
float frequencies[STEPS];
bool accents[STEPS];
bool slides[STEPS];
long taps[4];

// get the frequency in Hz for given semitone.
float note_to_frequency(int note,int octave)
{
  return 440.f*exp2f(note/12.0f+octave-5);
}

int midi_note_on=-1;
int midi_clock_cycle=0;
int midi_clock_last=0;
float analogs[8];
void loop()
{
  long time;
  long dt=0;
  // update cycle time (time into current sequence step)
  time=millis();
  dt=time-last_time;
  cycle+=dt;
  last_time = time;

  // read all digital buttons and provide some change state (digitals_click)
  bool digitals[]={!digitalRead(17),!digitalRead(15),!digitalRead(14),digitalRead(22)};
  for(int i=0;i<4; i++)
  {
    digitals_click[i]=digitals_click[i] || (digitals[i] && !digitals_last[i]);
    digitals_last[i]=digitals[i];
  }

  // read octave knob
  int octave=(digitalRead(12)<<3) + (digitalRead(11)<<2) + (digitalRead(10)<<1) + (digitalRead(9)<<0);
  
  // read all analog knobs
  int analogs_raw[]={analogRead(A19),analogRead(A18),analogRead(A17),analogRead(A16),analogRead(A15),analogRead(A14),analogRead(A5),analogRead(A6)};
  float threshold=1.f/1024.f;
  for(int i=0; i<8; i++){
    float target=(analogs_raw[i]/1023.f)*(1.f+2.f*threshold)-threshold;
    // filter a little bit to get a dead zone steady state
    if(abs(analogs[i]-target)>threshold)
      analogs[i]=max(0.f, min(1.f,analogs[i]*0.9f+target*0.1f));
//    Serial.print(analogs[i],3);Serial.print(" ");
  }
//  Serial.println();

  // read tempo tap button and adapt tempo after four clicks
  if(digitals_click[2])
  {
    digitals_click[2]=false;
    long four_bars=time-taps[2];
    cycle_length=base_cycle_length=four_bars/12;

    for(int i=2; i>=0; i--)
      taps[i+1]=taps[i];
    taps[0]=time;

    // also clear midi state to allow recover from stuck MIDI notes
    midi_note_on=-1;
  }

  // read capacitive touch keyboard
  for(int i=0; i<16; i++)
  {
    float total=capacities[i].get(2);
//    Serial.print(total); Serial.print(" ");
    if(abs(total)>100.f) {
      int next_step=(step+1)%STEPS;
      frequencies[next_step]=note_to_frequency(i,octave);
      accents[next_step]=digitals[0];
      slides [next_step]=digitals[1];
    }
  }
//  Serial.println();

  int trigger=false;
  // do MIDI events
  while(true){
    int incoming=0;
    if     (usbMIDI.read()) incoming=1;
    else if(MIDI   .read()) incoming=2;
    else break;

    int midiEvent=incoming==1 ? usbMIDI.getType() : MIDI.getType();
    int midiData1=incoming==1 ? usbMIDI.getData1(): MIDI.getData1();
    int midiData2=incoming==1 ? usbMIDI.getData2(): MIDI.getData2();
    
    // Serial.print("MIDI ");Serial.print(incoming); Serial.print(" "); Serial.print(midiEvent); Serial.print(" "); Serial.print(midiData1); Serial.print(" "); Serial.print(midiData2); Serial.println();

    // handle incoming MIDI notes, priority on bright ones
    // TODO check how other monophonic instruments handle MIDI
    if(midiEvent==usbMIDI.NoteOn && midiData1>midi_note_on){
      // read octave selection knob
      int transpose=-21-12*2;
      frequencies[step]=note_to_frequency(midiData1+transpose,octave);
      accents[step]=(midiData2>=100);
      slides [step]=false;
      trigger=true;   // trigger note
      cycle_length=0; // disengage sequencer
      midi_note_on=midiData1;
    }else if(midiEvent==usbMIDI.NoteOff && midiData1==midi_note_on)
      midi_note_on=-1;
    else if(midiEvent==usbMIDI.Clock && cycle_length>0){
      if(midi_clock_cycle%6==0)
      {
        step=(midi_clock_cycle/6+(STEPS-1))%STEPS;
        cycle=base_cycle_length=cycle_length=(time-midi_clock_last)*1.2f;
        midi_clock_last=time;
      }
      midi_clock_cycle++;
    }else if(midiEvent==usbMIDI.Start){
      cycle=cycle_length=base_cycle_length;
      midi_clock_cycle=0;
    }else if(midiEvent==usbMIDI.Stop){
      midi_clock_cycle=0;
      cycle_length=0;    
    }else if(midiEvent==usbMIDI.SongPosition){
      int pos=midiData1 + (midiData2<<7);
      midi_clock_cycle=pos;
    }
  };

  // do sequencer.
  if(cycle_length && cycle>=cycle_length){

    // allow to toggle the accent and slide state of the ongoing note at any time
    if(digitals_click[0]) accents[step]=!accents[step];
    digitals_click[0]=false;
    if(digitals_click[1]) slides [step]=!slides [step];
    digitals_click[1]=false;

    bool  last_slide    =slides[step];

    // Advance step to play next note by sequencer.
    step++;
    step=step % STEPS;

    // delete button, erase current note
    if(digitals[3]) {
      frequencies[step]=0;
      accents[step]=false;
      slides [step]=false;
    }

    // compute next cycle time
    cycle-=cycle_length;

    // compute next cycle length
    // swing even / odd beat if needed.
    cycle_length=base_cycle_length*(1.f-analogs[7]*((step%2==0) ? -0.5f : 0.5f));

    // trigger note
    if(frequencies[step] && !last_slide)
      trigger=true;

    // update beat LED
    digitalWrite(13,step%4==0);
  }  

  // start playing a new note
  if(trigger){
    // compute per-note synthesis parameters. Further parameters are updated later per tick.
    // for accented notes, TB-303 decay would be 200ms. We tie that to our cycle, so adjust to 200ms for 120bpm.
    float decay=accents[step] ? analogs[6]*3000.f : analogs[4]*3000.f;
    float accent=analogs[5];
    float accent_slew=200.f * (1.f+analogs[2]);
    // set per-note synthesis parameters.
    AudioNoInterrupts();
    vcfEnv.decay(slides[step] ? 10000.f : decay);
    accEnv.attack(accent_slew*0.2f);  // accent slew tied to 'resonance' pot like TB-303 does.
    accEnv.decay (accent_slew);
    if(accents[step]) accEnv.pulse(accent);
    vcfEnv.noteOn(1.f);
    vcaEnv.noteOn(1.f);
    AudioInterrupts();

    // update LEDs
    digitalWrite(18,accents[step]);
    digitalWrite(16,slides [step]);
  }

  // every tick (~5ms) compute some synthesis parameters.
  float frequency=frequencies[step];  
  if(slides[step]){
    // compute frequency by slide.
    // TODO we should use a signal-controlled oscilator to handle this updates in Audio engine.
    float next_f=frequencies[(step+1)%STEPS];
    float t=((float)cycle)/base_cycle_length;
    frequency=frequency*(1.f-t)+next_f*t;
  }
  float filter_cutoff    =(1.f-log2f(1024.f-analogs[1]*1023.f)/10.f)*4096.0f; // emulate log pot
  float filter_resonance =analogs[2]*4.0f;
  float filter_mod       =analogs[3]*1.0f;
  // bias filter_cutoff by filter_mod. We had an bias of -1 on the vcfMixer in the past,
  // resulting in a cutoff that closed fully even with filter_mod employed.
  filter_cutoff*=exp2f(1.f-filter_mod*7.f);
  float oscMix           =analogs[0];

  // update synthesis parameters.
  AudioNoInterrupts();
  if(frequency) oscSaw   .frequency(frequency);
  if(frequency) oscSquare.frequency(frequency);
  vcfMixer.gain(0,filter_mod);
  vcf1.resonance(filter_resonance);
  vcf1.frequency(filter_cutoff);
  vcf2.resonance(filter_resonance);
  vcf2.frequency(filter_cutoff);
  oscMixer.gain(0,1.-oscMix);
  oscMixer.gain(1,   oscMix);
  AudioInterrupts();

  // print CPU, memory usage and timing informations
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
