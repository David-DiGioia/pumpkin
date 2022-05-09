#pragma once

#include <functional>
#include <vector>
#include <array>
#include "SFML/Audio.hpp"

namespace pmk
{
	constexpr uint32_t AUDIO_CHUNKS_IN_FLIGHT{ 2 };
	constexpr uint32_t AUDIO_CHUNK_SIZE{ 2048 }; // Number of samples in a chunk.
	constexpr uint32_t SAMPLE_RATE{ 44100 };

	// Built in wave functions. In each case t is in [0, 1] and output is in [-1, 1].
	namespace wave
	{
		float Sin01(float t);

		float Square(float t);

		float Saw(float t);
	}

	struct Wave
	{
		// Periodic function f: [0, 1] -> [-1, 1].
		std::function<float(float t)> fundamental_wave;

		// Get amplitude of each multiple of fundamental frequency to add as a function of time.
		// Does not need to be periodic, f: (R+, Z+) -> [0, 1].
		std::function<float(float time, uint32_t freq_multiple)> harmonic_multipliers{ [](float t, uint32_t n) {
			return (float)(n == 1);
		} };

		float relative_frequency{ 1.0f }; // Frequency fundamental_wave relative to others in an instrument.
		uint32_t unison{ 1 };
		float unison_step_size{ 10.0f }; // In hertz.
	};

	class InstrumentAudioStream : public sf::SoundStream
	{
		virtual bool onGetData(Chunk& data);

		virtual void onSeek(sf::Time timeOffset);
	};

	class Instrument
	{
	public:
		Instrument();

		void Play();

		void Reset();

		// Push an audio chunk to buffer if there are less than AUDIO_CHUNKS_IN_FLIGHT already pushed to the buffer.
		void FrameUpdate();

		void AddWave(const Wave& wave);

		void SetFrequency(float frequency);

		void SetAmplitude(float amplitude);

		float GetTime() const;

		const std::array<sf::Int16, AUDIO_CHUNK_SIZE* AUDIO_CHUNKS_IN_FLIGHT>& GetAudioBuffer() const;

	private:
		// Returns true if the play position passed into a new audio chunk.
		bool AudioChunkChanged();

		void RecordAudioChunk(uint32_t chunk_index);

		std::vector<Wave> waves_{};
		sf::SoundBuffer sound_buffer_{};
		sf::Sound sound_{};
		std::array<sf::Int16, AUDIO_CHUNK_SIZE* AUDIO_CHUNKS_IN_FLIGHT> audio_buffer_{};

		float frequency_{ 200.0f };
		float amplitude_{ 0.1f };

		bool playing_{}; // True if this instrument is currently playing.
		uint32_t current_chunk_index_{}; // The index of the last chunk the audio was playing in.
	};

	class AudioEngine
	{
	public:
		void AddInstrument(Instrument* instrument);

		void Update();

	private:
		std::vector<Instrument*> instruments_;
	};
}
