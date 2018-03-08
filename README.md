Spine Synth 303

A somewhat TB-303-like subtractive synthesizer for Teensy 3.x ARM Cortex M4

Simple UI made of several pots, pushbuttons, LEDs and 16 touch sensitive keys (anything conductive will do).

Provides 16bit audio out via USB sound device and 12bit line-out via the internal DAC.

Provides MIDI-in via USB

Several parameters are choosen to behave more or less then a TB-303.

Some parameters are choosen to perform easier than a TB-303, eg. some envelope timings are tied to sequencer speed.
The sequencer does not follow the complicated step programming scheme, but only allows to enter notes
in realtime into the running sequence.

Some of the TB-303 unique behaviours are implemented in a most simple manner. 

We do not try to match the sound of the original device, but provide the basic qualities 
of it's sound and unique features the most simple emulations lack.

For details and build instrucions, see comments in spine_synth.ino.
