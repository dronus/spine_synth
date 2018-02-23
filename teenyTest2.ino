#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
 

// GUItool: begin automatically generated code
//AudioInputUSB            usb1;           //xy=138,124
AudioEffectEnvelope      env1;
AudioSynthWaveform       osc1;
AudioFilterStateVariable filter1;        //xy=340,125
AudioFilterStateVariable filter2;        //xy=343,180
AudioOutputAnalog        dac1;           //xy=546,73
AudioOutputUSB           usb2;           //xy=548,127
AudioConnection          patchCord0(osc1, 0, env1, 0);
AudioConnection          patchCord1(env1, 0, filter1, 0);
AudioConnection          patchCord2(env1, 0, filter2, 0);
AudioConnection          patchCord3(filter1, 0, usb2, 0);
AudioConnection          patchCord4(filter1, 0, dac1, 0);
AudioConnection          patchCord5(filter2, 0, usb2, 1);
// GUItool: end automatically generated code

#define FIVE_VOLT_TOLERANCE_WORKAROUND
#include <CapacitiveSensor.h>

CapacitiveSensor c1(8, 9);
CapacitiveSensor c2(8,10);



void setup(void)
{
  pinMode(22,INPUT_PULLDOWN);
  pinMode(23,INPUT_PULLDOWN);

//  AudioMemory(30);
  Serial.println("setup done");
  Serial.begin(9600);
  while (!Serial) ;

  AudioMemory(30);
  osc1.begin(WAVEFORM_SQUARE);
  osc1.pulseWidth(0.5);
  osc1.frequency(440.0);
  osc1.amplitude(1.0);
  Serial.println("setup done");
}
 
long last_time;
float analogs_slow[]={0.,0.,0.,0.,0.,0.};
float last_volume=0.;
int cycle=0;
int step =0;
float frequencies[16];
float volumes[16];
void loop()
{

  int analogs[]={analogRead(A5),analogRead(A6),analogRead(A7),c1.capacitiveSensor(64),c2.capacitiveSensor(64)};

  for(int i=0; i<5; i++)
    analogs_slow[i]=analogs_slow[i]*0.95+analogs[i]*0.05;
  
 /*
  for(int i=0; i<5; i++)
  {
    Serial.print(analogs[i]);
    Serial.print(" ");
  }
  byte digitals[]={digitalRead(22),digitalRead(23)};
  for(int i=0; i<2; i++)
  {
    Serial.print(digitals[i]);
    Serial.print(" ");
  }
  Serial.println();
*/
  float volume=analogs_slow[3]+analogs_slow[4];
  float frequency=(analogs_slow[3]-analogs_slow[4])/volume+0.5f;
  frequency*=100.0f;
  // Serial.print("f:");  Serial.print(frequency); Serial.println();

  volume-=5000.0f;
  if(volume<0.f) volume=0.f;
  volume/=5000.0f;


  int cycle_length=analogs_slow[0];

  long time=millis();
  long dt=time-last_time;
  last_time = time;

  cycle+=dt;
  if(cycle>cycle_length){
    step++;
    step=step % 16;

    if(volume>0) frequencies[step]=frequency;
    if(volume>0) volumes[step]    =volume;
    osc1.frequency  (frequencies[step]);
    osc1.amplitude  (volumes[step]);

    if(volumes[step]>0) env1.noteOn();
    else                env1.noteOff();

    digitalWrite(13,step%2==0);

    cycle-=cycle_length;
  }


  AudioNoInterrupts();
  filter1.resonance(analogs_slow[1]*5.0f/1024.0f);
  filter2.resonance(analogs_slow[1]*5.0f/1024.0f);
  filter1.frequency((analogs_slow[2])*3.0f);
  filter2.frequency((analogs_slow[2])*3.0f);
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
}
