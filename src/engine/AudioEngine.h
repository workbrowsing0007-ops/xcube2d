#ifndef __AUDIO_ENGINE_H__
#define __AUDIO_ENGINE_H__

#include <SDL3_mixer/SDL_mixer.h>

#include "EngineCommon.h"

class AudioEngine {
	friend class XCube2Engine;
	private:
		AudioEngine();
		MIX_Mixer *mixer;
		bool soundOn;
		int volume;
	public:
		~AudioEngine();
		void toggleSound();

		/**
		* Controls the volume of all sounds
		* @param volume - in the range [0..128]
		*/
		void setSoundVolume(const int &);
		int getSoundVolume();

		void playSound(MIX_Audio * sound);

		/**
		* Call this to manually specify the volume of the sound
		*
		* @param sound - the sound to play
		* @param volume - the volume at which to play in range [0..128]
		*/
		void playSound(MIX_Audio * sound, const int & _volume);

		/**
		* Plays mp3 file given amount of times
		*
		* @param mp3 - the file to play
		* @param times - number of times, -1 will play indefinitely
		*/
		void playMP3(MIX_Audio * mp3, const int & times);

		MIX_Mixer* getMixer() { return mixer; }
};

#endif