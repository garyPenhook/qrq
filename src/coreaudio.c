/*
Copyright (c) 2011 Marc Vaillant

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#define kOutputBus 0

extern long samplerate;

static const int *_pcm;
static int _pcm_size;
static int _index;
static int _playback_complete = 1;
static AudioComponentInstance _audio_unit = NULL;

static OSStatus playbackCallback(void *inRefCon, 
                                  AudioUnitRenderActionFlags *ioActionFlags, 
                                  const AudioTimeStamp *inTimeStamp, 
                                  UInt32 inBusNumber, 
                                  UInt32 inNumberFrames, 
                                  AudioBufferList *ioData) 
{
	UInt32 i;

	(void)inRefCon;
	(void)ioActionFlags;
	(void)inTimeStamp;
	(void)inBusNumber;
	(void)inNumberFrames;
	if (ioData == NULL) {
		return paramErr;
	}

	for (i = 0; i < ioData->mNumberBuffers; ++i) {
		AudioBuffer *audio_buffer = &ioData->mBuffers[i];
		int samples_left = _pcm_size - _index;
		size_t capacity = audio_buffer->mDataByteSize / sizeof(*_pcm);
		size_t copied = 0;

		if (samples_left > 0 && audio_buffer->mData != NULL) {
			copied = (size_t)samples_left < capacity ? (size_t)samples_left : capacity;
			memcpy(audio_buffer->mData, &_pcm[_index], copied * sizeof(*_pcm));
			_index += (int)copied;
		}
		if (audio_buffer->mData != NULL && copied < capacity) {
			memset((unsigned char *)audio_buffer->mData + copied * sizeof(*_pcm),
					0, (capacity - copied) * sizeof(*_pcm));
		}
	}
	if (_index >= _pcm_size) {
		/* The render callback must not block or stop its own AudioUnit. */
		__atomic_store_n(&_playback_complete, 1, __ATOMIC_RELEASE);
	}
	return noErr;
}

int write_audio(void *dummy, const int *pcm, int size) {
	const struct timespec poll_interval = {0, 1000000};
	OSStatus status;

	(void)dummy;
	if (_audio_unit == NULL || pcm == NULL || size < 0 ||
			size % (int)sizeof(*pcm) != 0) {
		errno = EINVAL;
		return -1;
	}
	_pcm = pcm;
	_pcm_size = size / (int)sizeof(*pcm);
	_index = 0;
	__atomic_store_n(&_playback_complete, 0, __ATOMIC_RELEASE);
	status = AudioOutputUnitStart(_audio_unit);
	if (status != noErr) {
		fprintf(stderr, "AudioOutputUnitStart failed: %ld\n", (long)status);
		__atomic_store_n(&_playback_complete, 1, __ATOMIC_RELEASE);
		return -1;
	}
	while (!__atomic_load_n(&_playback_complete, __ATOMIC_ACQUIRE)) {
		(void)nanosleep(&poll_interval, NULL);
	}
	status = AudioOutputUnitStop(_audio_unit);
	if (status != noErr) {
		fprintf(stderr, "AudioOutputUnitStop failed: %ld\n", (long)status);
		return -1;
	}
	return 0;
}

int close_audio(void *cookie) {
	OSStatus status;

	(void)cookie;
	if (_audio_unit == NULL) {
		return 0;
	}
	status = AudioOutputUnitStop(_audio_unit);
	return status == noErr ? 0 : -1;
}

void close_dsp(void *unused) {
	(void)unused;
	if (_audio_unit == NULL) {
		return;
	}
	(void)AudioUnitUninitialize(_audio_unit);
	(void)AudioComponentInstanceDispose(_audio_unit);
	_audio_unit = NULL;
}

void *open_dsp(char *dummy) {
	AudioComponentDescription desc = {0};
	AudioStreamBasicDescription audio_format = {0};
	AURenderCallbackStruct callback = {0};
	AudioComponent input_component;
	OSStatus status;
	UInt32 flag = 1;

	(void)dummy;
	if (_audio_unit != NULL) {
		return _audio_unit;
	}

	desc.componentType = kAudioUnitType_Output;
#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	input_component = AudioComponentFindNext(NULL, &desc);
	if (input_component == NULL) {
		fprintf(stderr, "No Core Audio output component is available.\n");
		return NULL;
	}
	status = AudioComponentInstanceNew(input_component, &_audio_unit);
	if (status != noErr) {
		goto fail;
	}
	status = AudioUnitSetProperty(_audio_unit, kAudioOutputUnitProperty_EnableIO,
			kAudioUnitScope_Output, kOutputBus, &flag, sizeof(flag));
	if (status != noErr) {
		goto fail;
	}

	audio_format.mSampleRate = samplerate;
	audio_format.mFormatID = kAudioFormatLinearPCM;
	audio_format.mFormatFlags = kAudioFormatFlagIsSignedInteger |
			kAudioFormatFlagIsPacked;
	audio_format.mFramesPerPacket = 1;
	audio_format.mChannelsPerFrame = 2;
	audio_format.mBitsPerChannel = 16;
	audio_format.mBytesPerPacket = 4;
	audio_format.mBytesPerFrame = 4;
	status = AudioUnitSetProperty(_audio_unit, kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Input, kOutputBus, &audio_format,
			sizeof(audio_format));
	if (status != noErr) {
		goto fail;
	}

	callback.inputProc = playbackCallback;
	status = AudioUnitSetProperty(_audio_unit,
			kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global,
			kOutputBus, &callback, sizeof(callback));
	if (status != noErr) {
		goto fail;
	}
	status = AudioUnitInitialize(_audio_unit);
	if (status != noErr) {
		goto fail;
	}
	return _audio_unit;

fail:
	fprintf(stderr, "Core Audio initialization failed: %ld\n", (long)status);
	if (_audio_unit != NULL) {
		(void)AudioComponentInstanceDispose(_audio_unit);
		_audio_unit = NULL;
	}
	return NULL;
}

//int main()
//{    
//
//  //generate pcm tone  freq = 800, duration = 5s, rise/fall time = 5ms
//
//  generateTone(_pcm, 800, 500, SAMPLE_RATE, 5, 0.5);
//  _index = 0;
//
//  OSStatus status;
//  AudioComponentInstance audioUnit;
//
//  // Describe audio component
//  AudioComponentDescription desc;
//  desc.componentType = kAudioUnitType_Output;
//  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
//  desc.componentFlags = 0;
//  desc.componentFlagsMask = 0;
//  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
//
//  // Get component
//  AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
//
//  // Get audio units
//  status = AudioComponentInstanceNew(inputComponent, &audioUnit);
//  //checkStatus(status);
//
//  UInt32 flag = 1;
//  // Enable IO for playback
//  status = AudioUnitSetProperty(audioUnit, 
//				  kAudioOutputUnitProperty_EnableIO, 
//				  kAudioUnitScope_Output, 
//				  kOutputBus,
//				  &flag, 
//				  sizeof(flag));
//  //checkStatus(status);
//
//  // Describe format
//
//  AudioStreamBasicDescription audioFormat;
//  audioFormat.mSampleRate = SAMPLE_RATE;
//  audioFormat.mFormatID	= kAudioFormatLinearPCM;
//  audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
//  audioFormat.mFramesPerPacket = 1;
//  audioFormat.mChannelsPerFrame = 2;
//  audioFormat.mBitsPerChannel = 16;
//  audioFormat.mBytesPerPacket = 4;
//  audioFormat.mBytesPerFrame = 4;
//
//  // Apply format
//
//  status = AudioUnitSetProperty(audioUnit, 
//				  kAudioUnitProperty_StreamFormat, 
//				  kAudioUnitScope_Input, 
//				  kOutputBus, 
//				  &audioFormat, 
//				  sizeof(audioFormat));
////  checkStatus(status);
//
//  // Set output callback
//  AURenderCallbackStruct callbackStruct;
//  callbackStruct.inputProc = playbackCallback;
//  callbackStruct.inputProcRefCon = NULL;
//  status = AudioUnitSetProperty(audioUnit, 
//				  kAudioUnitProperty_SetRenderCallback, 
//				  kAudioUnitScope_Global, 
//				  kOutputBus,
//				  &callbackStruct, 
//				  sizeof(callbackStruct));
//
//  // Initialize
//  status = AudioUnitInitialize(audioUnit);
//  status = AudioOutputUnitStart(audioUnit);
//
//  // sleep
//  
//  sleep(2.0);
//  return 1;
//}
