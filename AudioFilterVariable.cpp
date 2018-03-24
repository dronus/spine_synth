/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "AudioFilterVariable.h"
#include <AudioStream_F32.h>

#if defined(KINETISK)
/*
void AudioFilterStateVariable::update_fixed(const int16_t *in,
	int16_t *lp, int16_t *bp, int16_t *hp)
{
	const int16_t *end = in + AUDIO_BLOCK_SAMPLES;
	int32_t input, inputprev;
	int32_t lowpass, bandpass, highpass;
	int32_t lowpasstmp, bandpasstmp, highpasstmp;
	int32_t fmult, damp;

	fmult = setting_fmult;
	damp = setting_damp;
	inputprev = state_inputprev;
	lowpass = state_lowpass;
	bandpass = state_bandpass;
	do {
		input = (*in++) << 12;
		lowpass = lowpass + MULT(fmult, bandpass);
		highpass = ((input + inputprev)>>1) - lowpass - MULT(damp, bandpass);
		inputprev = input;
		bandpass = bandpass + MULT(fmult, highpass);
		lowpasstmp = lowpass;
		bandpasstmp = bandpass;
		highpasstmp = highpass;
		lowpass = lowpass + MULT(fmult, bandpass);
		highpass = input - lowpass - MULT(damp, bandpass);
		bandpass = bandpass + MULT(fmult, highpass);
		lowpasstmp = signed_saturate_rshift(lowpass+lowpasstmp, 16, 13);
		bandpasstmp = signed_saturate_rshift(bandpass+bandpasstmp, 16, 13);
		highpasstmp = signed_saturate_rshift(highpass+highpasstmp, 16, 13);
		*lp++ = lowpasstmp;
		*bp++ = bandpasstmp;
		*hp++ = highpasstmp;
	} while (in < end);
	state_inputprev = inputprev;
	state_lowpass = lowpass;
	state_bandpass = bandpass;
}
*/

__inline__ float clamp(float x)
{
  return min(1.f,max(-1.f,x));
}

void AudioFilterStateVariableF32::update_variable(const float *in,
	const float *ctl, float *lp, float *bp, float *hp)
{
	const float *end = in + AUDIO_BLOCK_SAMPLES;
	float input, inputprev, control;
	float lowpass, bandpass, highpass;
	float lowpasstmp, bandpasstmp, highpasstmp;
	float fcenter, fmult, damp, octavemult;

	fcenter = setting_fcenter;
	octavemult = setting_octavemult;
	damp = setting_damp;
	inputprev = state_inputprev;
	lowpass = state_lowpass;
	bandpass = state_bandpass;
  int i=0;
	// compute fmult using control input, fcenter and octavemult
  float fmult0=clamp(ctl ? exp2f(octavemult * ctl[0                    ])*fcenter : fcenter);
  float fmult1=clamp(ctl ? exp2f(octavemult * ctl[AUDIO_BLOCK_SAMPLES-1])*fcenter : fcenter);
	do {
    float t=((float)i++)/AUDIO_BLOCK_SAMPLES;
    fmult=(1.f-t)*fmult0 + t*fmult1;
		// now do the state variable filter as normal, using fmult
		input = *in++;
		lowpass = lowpass + fmult * bandpass;
		highpass = (input + inputprev)/2.f - lowpass - damp * bandpass;
		inputprev = input;
		bandpass = bandpass + (fmult * highpass);
		lowpasstmp = lowpass;
		bandpasstmp = bandpass;
		highpasstmp = highpass;
		lowpass = lowpass + fmult * bandpass;
		highpass = input - lowpass - damp * bandpass;
		bandpass = bandpass + fmult * highpass;
/*
    lowpass=clamp(lowpass);
    bandpass=clamp(bandpass);
    highpass=clamp(highpass);
*/
		lowpasstmp =  (lowpass+lowpasstmp)  /2.f;
		bandpasstmp = (bandpass+bandpasstmp)/2.f;
		highpasstmp = (highpass+highpasstmp)/2.f;
    
		*lp++ = clamp(lowpasstmp);
		*bp++ = clamp(bandpasstmp);
		*hp++ = clamp(highpasstmp);
	} while (in < end);
	state_inputprev = inputprev;
	state_lowpass = lowpass;
	state_bandpass = bandpass;
}


void AudioFilterStateVariableF32::update(void)
{
	audio_block_f32_t *input_block=NULL, *control_block=NULL;
	audio_block_f32_t *lowpass_block=NULL, *bandpass_block=NULL, *highpass_block=NULL;

	input_block = receiveReadOnly_f32(0);
	control_block = receiveReadOnly_f32(1);
	if (!input_block) {
		if (control_block) AudioStream_F32::release(control_block);
		return;
	}
	lowpass_block = allocate_f32();
	if (!lowpass_block) {
		AudioStream_F32::release(input_block);
		if (control_block) AudioStream_F32::release(control_block);
		return;
	}
	bandpass_block = allocate_f32();
	if (!bandpass_block) {
		AudioStream_F32::release(input_block);
		AudioStream_F32::release(lowpass_block);
		if (control_block) AudioStream_F32::release(control_block);
		return;
	}
	highpass_block = allocate_f32();
	if (!highpass_block) {
		AudioStream_F32::release(input_block);
		AudioStream_F32::release(lowpass_block);
		AudioStream_F32::release(bandpass_block);
		if (control_block) release(control_block);
		return;
	}

		update_variable(input_block->data,
			 control_block ? control_block->data : NULL, 
			 lowpass_block->data,
			 bandpass_block->data,
			 highpass_block->data);
		AudioStream_F32::release(control_block);

	AudioStream_F32::release(input_block);
	AudioStream_F32::transmit(lowpass_block, 0);
	AudioStream_F32::release(lowpass_block);
	AudioStream_F32::transmit(bandpass_block, 1);
	AudioStream_F32::release(bandpass_block);
	AudioStream_F32::transmit(highpass_block, 2);
	AudioStream_F32::release(highpass_block);
	return;
}

#elif defined(KINETISL)

void AudioFilterStateVariableF32::update(void)
{
	audio_block_f32_t *block;

	block = receiveReadOnly_f32(0);
	if (block) release(block);
	block = receiveReadOnly_f32(1);
	if (block) release(block);
}

#endif

