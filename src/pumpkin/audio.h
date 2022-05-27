#pragma once

#include <functional>
#include <vector>
#include <array>
#include <mutex>
#include "SFML/Audio.hpp"

namespace pmk
{
	constexpr uint32_t AUDIO_CHUNK_COUNT{ 40 };
	constexpr uint32_t AUDIO_CHUNK_SIZE{ 1024 }; // Number of samples in a chunk.
	constexpr uint32_t SAMPLE_RATE{ 44100 };
	constexpr uint32_t MAX_HARMONIC_MULTIPLE{ 8 };
	constexpr uint32_t AUDIO_BUF_SIZE{ AUDIO_CHUNK_COUNT * AUDIO_CHUNK_SIZE };
	constexpr float NOTE_ACTIVATION_CUTOFF{ 0.01f }; // Notes below this activation are considered unactive.

	typedef std::array<sf::Int16, AUDIO_BUF_SIZE> AudioBuffer;

	enum class Note
	{
		C_0 = 0,
		C_SHARP_D_FLAT_0,
		D_0,
		D_SHARP_E_FLAT_0,
		E_0,
		F_0,
		F_SHARP_G_FLAT_0,
		G_0,
		G_SHARP_A_FLAT_0,
		A_0,
		A_SHARP_B_FLAT_0,
		B_0,
		C_1,
		C_SHARP_D_FLAT_1,
		D_1,
		D_SHARP_E_FLAT_1,
		E_1,
		F_1,
		F_SHARP_G_FLAT_1,
		G_1,
		G_SHARP_A_FLAT_1,
		A_1,
		A_SHARP_B_FLAT_1,
		B_1,
		C_2,
		C_SHARP_D_FLAT_2,
		D_2,
		D_SHARP_E_FLAT_2,
		E_2,
		F_2,
		F_SHARP_G_FLAT_2,
		G_2,
		G_SHARP_A_FLAT_2,
		A_2,
		A_SHARP_B_FLAT_2,
		B_2,
		C_3,
		C_SHARP_D_FLAT_3,
		D_3,
		D_SHARP_E_FLAT_3,
		E_3,
		F_3,
		F_SHARP_G_FLAT_3,
		G_3,
		G_SHARP_A_FLAT_3,
		A_3,
		A_SHARP_B_FLAT_3,
		B_3,
		C_4,
		C_SHARP_D_FLAT_4,
		D_4,
		D_SHARP_E_FLAT_4,
		E_4,
		F_4,
		F_SHARP_G_FLAT_4,
		G_4,
		G_SHARP_A_FLAT_4,
		A_4,
		A_SHARP_B_FLAT_4,
		B_4,
		C_5,
		C_SHARP_D_FLAT_5,
		D_5,
		D_SHARP_E_FLAT_5,
		E_5,
		F_5,
		F_SHARP_G_FLAT_5,
		G_5,
		G_SHARP_A_FLAT_5,
		A_5,
		A_SHARP_B_FLAT_5,
		B_5,
		C_6,
		C_SHARP_D_FLAT_6,
		D_6,
		D_SHARP_E_FLAT_6,
		E_6,
		F_6,
		F_SHARP_G_FLAT_6,
		G_6,
		G_SHARP_A_FLAT_6,
		A_6,
		A_SHARP_B_FLAT_6,
		B_6,
		C_7,
		C_SHARP_D_FLAT_7,
		D_7,
		D_SHARP_E_FLAT_7,
		E_7,
		F_7,
		F_SHARP_G_FLAT_7,
		G_7,
		G_SHARP_A_FLAT_7,
		A_7,
		A_SHARP_B_FLAT_7,
		B_7,
		C_8,
		C_SHARP_D_FLAT_8,
		D_8,
		D_SHARP_E_FLAT_8,
		E_8,
		F_8,
		F_SHARP_G_FLAT_8,
		G_8,
		G_SHARP_A_FLAT_8,
		A_8,
		A_SHARP_B_FLAT_8,
		B_8,
		NOTES_COUNT,
	};

	const std::array<const char*, (size_t)Note::NOTES_COUNT> note_names{
		"C_0 = 0", "C_SHARP_D_FLAT_0", "D_0", "D_SHARP_E_FLAT_0", "E_0", "F_0", "F_SHARP_G_FLAT_0", "G_0", "G_SHARP_A_FLAT_0", "A_0",
		"A_SHARP_B_FLAT_0", "B_0", "C_1", "C_SHARP_D_FLAT_1", "D_1", "D_SHARP_E_FLAT_1", "E_1", "F_1", "F_SHARP_G_FLAT_1", "G_1",
		"G_SHARP_A_FLAT_1", "A_1", "A_SHARP_B_FLAT_1", "B_1", "C_2", "C_SHARP_D_FLAT_2", "D_2", "D_SHARP_E_FLAT_2", "E_2", "F_2",
		"F_SHARP_G_FLAT_2", "G_2", "G_SHARP_A_FLAT_2", "A_2", "A_SHARP_B_FLAT_2", "B_2", "C_3", "C_SHARP_D_FLAT_3", "D_3", "D_SHARP_E_FLAT_3",
		"E_3", "F_3", "F_SHARP_G_FLAT_3", "G_3", "G_SHARP_A_FLAT_3", "A_3", "A_SHARP_B_FLAT_3", "B_3", "C_4", "C_SHARP_D_FLAT_4", "D_4",
		"D_SHARP_E_FLAT_4", "E_4", "F_4", "F_SHARP_G_FLAT_4", "G_4", "G_SHARP_A_FLAT_4", "A_4", "A_SHARP_B_FLAT_4", "B_4", "C_5", "C_SHARP_D_FLAT_5",
		"D_5", "D_SHARP_E_FLAT_5", "E_5", "F_5", "F_SHARP_G_FLAT_5", "G_5", "G_SHARP_A_FLAT_5", "A_5", "A_SHARP_B_FLAT_5", "B_5", "C_6",
		"C_SHARP_D_FLAT_6", "D_6", "D_SHARP_E_FLAT_6", "E_6", "F_6", "F_SHARP_G_FLAT_6", "G_6", "G_SHARP_A_FLAT_6", "A_6", "A_SHARP_B_FLAT_6", "B_6",
		"C_7", "C_SHARP_D_FLAT_7", "D_7", "D_SHARP_E_FLAT_7", "E_7", "F_7", "F_SHARP_G_FLAT_7", "G_7", "G_SHARP_A_FLAT_7", "A_7", "A_SHARP_B_FLAT_7",
		"B_7", "C_8", "C_SHARP_D_FLAT_8", "D_8", "D_SHARP_E_FLAT_8", "E_8", "F_8", "F_SHARP_G_FLAT_8", "G_8", "G_SHARP_A_FLAT_8", "A_8",
		"A_SHARP_B_FLAT_8", "B_8",
	};

	// The indices of these frequencies are given by the Notes enum.
	const std::array<float, (size_t)Note::NOTES_COUNT> note_frequencies{
		16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87, 32.70, 34.65, 36.71, 38.89, 41.20, 43.65,
		46.25, 49.00, 51.91, 55.00, 58.27, 61.74, 65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47,
		130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94, 261.63, 277.18, 293.66, 311.13,
		329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88, 523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99,
		830.61, 880.00, 932.33, 987.77, 1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760.00, 1864.66,
		1975.53, 2093.00, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07, 4186.01,
		4434.92, 4698.63, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13
	};

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
	};

	class Instrument : public sf::SoundStream
	{
	public:
		Instrument();

		Instrument(const Instrument& other);

		void Play();

		void Stop();

		void PlayNotes(const std::vector<Note>& notes);

		void Reset();

		// Push an audio chunk to buffer if there are less than AUDIO_CHUNKS_IN_FLIGHT already pushed to the buffer.
		void FrameUpdate();

		void AddWave(const Wave& wave);

		void SetAmplitude(float amplitude);

		void SetUnison(uint32_t unison);

		void SetUnisonRadius(float radius);

		void SetAttack(const std::function<float(float t)>& func, float duration);

		void SetSustain(const std::function<float(float t)>& func, float duration);

		void SetRelease(const std::function<float(float t)>& func, float duration);

		// Get the envelope amplitude multiplier in range [0, 1] for a given note.
		float EvaluateEnvelope(Note note, float record_time);

		// Get total time audio has been playing.
		float GetTime() const;

		// Get time modulo buffer duration. This is useful for play head indicator on timeline.
		float GetBufferTime() const;

		const AudioBuffer& GetAudioBuffer() const;

		float GetNoteActivation(); // temp
		float GetNoteTime(); // temp
		float GetNoteReleaseAmplitude(); // temp

	private:
		void RecordAudioChunk();

		void UpdateTimes();

		void NextChunk();

		virtual bool onGetData(Chunk& data);

		virtual void onSeek(sf::Time timeOffset);

		std::vector<Wave> waves_{};

		float amplitude_{ 0.1f };

		// The audio stream is run on a different thread by SFML so we double buffer the audio,
		// while it's being read from one half of the buffer we write to the other.
		AudioBuffer audio_buffer_{};
		bool ready_to_write_{ false }; // True when the chunk at chunk_index_ is ready to be written to by main thread.
		uint32_t chunk_index_{ 0 }; // The index of the chunk next in line to be used by the stream.
		std::mutex mutex_{}; // Needed since onGetData() function is processed on separate thread.
		uint32_t sample_index_{ 0 }; // Monotonically increasing, to avoid discontinuity when wrapping around audio buffer.
		uint32_t unison_{ 1 };
		float unison_radius_{ 0.1f }; // In interval [0, 1] of how much oscillation period is sampled from.

		std::function<float(float t)> attack_func_{};
		std::function<float(float t)> sustain_func_{};
		std::function<float(float t)> release_func_{};
		float attack_duration_{};
		float sustain_duration_{};
		float release_duration_{};

		std::array<float, (size_t)Note::NOTES_COUNT> note_activations_{}; // Each note is in [0, 1] representing activation. Using keyboard, maybe it jumps straight to 1?
		std::array<float, (size_t)Note::NOTES_COUNT> note_previous_activations_{};
		std::array<float, (size_t)Note::NOTES_COUNT> note_times_{}; // How long note has been active / deactive.
		std::array<float, (size_t)Note::NOTES_COUNT> note_last_envelope_{};
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
