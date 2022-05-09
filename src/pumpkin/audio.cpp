#include "audio.h"

#include <algorithm>
#include <cmath>
#include "logger.h"

namespace pmk
{
	constexpr float TWO_PI{ 6.28318530718f };
	constexpr uint32_t MAX_HARMONIC_MULTIPLE{ 16 };

	template <typename Func>
	int16_t SampleWave(uint32_t tick, float frequency, float amplitude, Func f)
	{
		// Sampling frequency is the number of samples (ticks) per second, so dividing
		// by number of cycles in a second (frequency) gives us the number of ticks in a cycle.
		float ticks_per_cycle{ SAMPLE_RATE / frequency };

		// Ratio of where we sample from one cycle of the sine wave. In the range [0, 1].
		float cycle_ratio{ tick / ticks_per_cycle };

		// We only require f to be defind on [0, 1] so we consider just the fractional part of the float.
		float integer_part{};
		cycle_ratio = std::modff(cycle_ratio, &integer_part);

		// Amplitude is in the range [0, 1] so we convert the amplitude to the full range
		// of a 16 bit signed integer.
		int16_t amplitude_discrete{ (int16_t)(std::numeric_limits<int16_t>::max() * amplitude) };

		// Convert the the wave sample from [-1, 1] to the full range of int16_t.
		return (int16_t)(amplitude_discrete * f(cycle_ratio));
	}

	Instrument::Instrument()
	{
		sound_.setBuffer(sound_buffer_);
		sound_.setLoop(true);
		RecordAudioChunk(0);
	}

	void Instrument::Play()
	{
		playing_ = true;
		sound_.play();
	}

	void Instrument::Reset()
	{
		waves_.clear();
	}

	void Instrument::FrameUpdate()
	{
		if (!playing_) {
			return;
		}

		if (AudioChunkChanged())
		{
			uint32_t next_index{ (current_chunk_index_ + 1) % AUDIO_CHUNKS_IN_FLIGHT };
			auto play_offset{ sound_.getPlayingOffset() };
			RecordAudioChunk(next_index);
			sound_.setPlayingOffset(play_offset);
			sound_.play();
		}
	}

	void Instrument::AddWave(const Wave& wave)
	{
		waves_.push_back(wave);
	}

	void Instrument::SetFrequency(float frequency)
	{
		frequency_ = frequency;
	}

	void Instrument::SetAmplitude(float amplitude)
	{
		amplitude_ = amplitude;
	}

	float Instrument::GetTime() const
	{
		return sound_.getPlayingOffset().asSeconds();
	}

	const std::array<sf::Int16, AUDIO_CHUNK_SIZE* AUDIO_CHUNKS_IN_FLIGHT>& Instrument::GetAudioBuffer() const
	{
		return audio_buffer_;
	}

	bool Instrument::AudioChunkChanged()
	{
		float current_time{ sound_.getPlayingOffset().asSeconds() };
		float audio_chunk_time{ AUDIO_CHUNK_SIZE / (float)SAMPLE_RATE };
		uint32_t chunk_index{ (uint32_t)(current_time / audio_chunk_time) };

		if (chunk_index != current_chunk_index_)
		{
			current_chunk_index_ = chunk_index;
			return true;
		}

		return false;

	}

	void Instrument::RecordAudioChunk(uint32_t chunk_index)
	{
		uint32_t start_index = chunk_index * AUDIO_CHUNK_SIZE;

		for (uint32_t i{ start_index }; i < start_index + AUDIO_CHUNK_SIZE; ++i)
		{
			audio_buffer_[i] = 0;
			float seconds{ i / (float)SAMPLE_RATE };

			for (const Wave& wave : waves_)
			{
				for (uint32_t j{ 1 }; j <= MAX_HARMONIC_MULTIPLE; ++j)
				{
					audio_buffer_[i] += SampleWave(
						i,
						frequency_ * wave.relative_frequency * j,
						amplitude_ * wave.harmonic_multipliers(seconds, j),
						wave.fundamental_wave
					);
				}
			}
		}

		sound_buffer_.loadFromSamples(audio_buffer_.data(), audio_buffer_.size(), 1, SAMPLE_RATE);
	}

	namespace wave
	{
		float Sin01(float t)
		{
			return std::sinf(t * TWO_PI);
		}

		float Square(float t)
		{
			return (t > 0.5) ? 1.0 : -1.0;
		}

		float Saw(float t)
		{
			return 2.0f * t - 1.0f;
		}
	}

	void AudioEngine::AddInstrument(Instrument* instrument)
	{
		instruments_.push_back(instrument);
	}

	void AudioEngine::Update()
	{
		for (Instrument* instrument : instruments_) {
			instrument->FrameUpdate();
		}
	}

	/*
	// Get a single audio sample from a sinewave.
	//
	// tick - The index of the sample. Every 44100 samples is one second of audio.
	// frequency - The frequency of the wave that's being sampled from.
	// amplitude - The volume of the sample. Should be in the range [0, 1].
	int16_t SineWave(uint32_t tick, float frequency, float amplitude)
	{
		return SampleWave(tick, frequency, amplitude, std::sinf);
	}

	int16_t CustomWave(uint32_t tick, float frequency, float amplitude, const std::vector<ImVec2>& curve_data)
	{
		return SampleWave(tick, frequency, amplitude, [&](float x) {
			float integer_part{};
			return ImGui::CurveValueSmoothAudio(std::modff(x, &integer_part), curve_data.size(), curve_data.data());
			});
	}

	void EditorGui::PlayTestSound()
	{
		float seconds{ 2.0f };

		std::vector<sf::Int16> samples(seconds * SAMPLE_RATE);

		for (uint32_t i{ 0 }; i < samples.size(); ++i)
		{
			float freq{ 200.0f };
			samples[i] = CustomWave(i, freq, 0.4f, curve_editor_data_);
		}

		sound_buffer_.loadFromSamples(samples.data(), samples.size(), 1, SAMPLE_RATE);

		sound_.setBuffer(sound_buffer_);
		sound_.play();
	}
	*/
}
