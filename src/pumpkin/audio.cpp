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
		// Call the parent class's initialize function.
		initialize(1, SAMPLE_RATE);
	}

	void Instrument::Play()
	{
		// Record first chunk before playing so we have something to play.
		RecordAudioChunk();
		play(); // Parent function to play audio stream.
	}

	void Instrument::Reset()
	{
		waves_.clear();
	}

	void Instrument::FrameUpdate()
	{
		if (getStatus() != sf::SoundSource::Playing) {
			return;
		}

		if (ready_to_write_)
		{
			RecordAudioChunk();
			mutex_.lock();
			ready_to_write_ = false;
			mutex_.unlock();
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
		return getPlayingOffset().asSeconds();
	}

	const AudioBuffer& Instrument::GetAudioBuffer() const
	{
		return audio_buffer_;
	}

	void Instrument::RecordAudioChunk()
	{
		uint32_t start_index = chunk_index_ * AUDIO_CHUNK_SIZE;

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
	}

	void Instrument::NextChunk()
	{
		chunk_index_ = (chunk_index_ + 1) % AUDIO_CHUNK_COUNT;
	}

	bool Instrument::onGetData(Chunk& data)
	{
		data.sampleCount = AUDIO_CHUNK_SIZE;
		data.samples = &audio_buffer_[(size_t)chunk_index_ * AUDIO_CHUNK_SIZE];

		NextChunk();
		mutex_.lock();
		ready_to_write_ = true;
		mutex_.unlock();
		return true;
	}

	void Instrument::onSeek(sf::Time timeOffset)
	{
		// TODO: Maybe do something here...
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
}
