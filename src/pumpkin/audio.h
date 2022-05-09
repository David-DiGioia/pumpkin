#pragma once

#include <functional>
#include <vector>
#include <array>
#include <mutex>
#include "SFML/Audio.hpp"

namespace pmk
{
	constexpr uint32_t AUDIO_CHUNK_COUNT{ 2 };
	constexpr uint32_t AUDIO_CHUNK_SIZE{ 2048 }; // Number of samples in a chunk.
	constexpr uint32_t SAMPLE_RATE{ 44100 };
	constexpr uint32_t MAX_HARMONIC_MULTIPLE{ 8 };
	constexpr uint32_t AUDIO_BUF_SIZE{ AUDIO_CHUNK_COUNT * AUDIO_CHUNK_SIZE };

	typedef std::array<sf::Int16, AUDIO_BUF_SIZE> AudioBuffer;

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

	class Instrument : public sf::SoundStream
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

		// Get total time audio has been playing.
		float GetTime() const;

		// Get time modulo buffer duration. This is useful for play head indicator on timeline.
		float GetBufferTime() const;

		const AudioBuffer& GetAudioBuffer() const;

	private:
		void RecordAudioChunk();

		void NextChunk();

		virtual bool onGetData(Chunk& data);

		virtual void onSeek(sf::Time timeOffset);

		std::vector<Wave> waves_{};

		float frequency_{ 200.0f };
		float amplitude_{ 0.1f };

		// The audio stream is run on a different thread by SFML so we double buffer the audio,
		// while it's being read from one half of the buffer we write to the other.
		AudioBuffer audio_buffer_{};
		bool ready_to_write_{ false }; // True when the chunk at chunk_index_ is ready to be written to by main thread.
		uint32_t chunk_index_{ 0 }; // The index of the chunk next in line to be used by the stream.
		std::mutex mutex_{}; // Needed since onGetData() function is processed on separate thread.
		uint32_t sample_index_{ 0 }; // Monotonically increasing, to avoid discontinuity when wrapping around audio buffer.
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
