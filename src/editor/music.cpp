#include "music.h"

pmk::Note NextStep(pmk::Note note, uint32_t step_size)
{
	return (pmk::Note)((uint32_t)note + step_size);
}

std::vector<pmk::Note> GetNotesFromInput(pmk::Note scale, ScaleType scale_type)
{
	std::vector<pmk::Note> notes{};
	uint32_t whole{ 2 };
	uint32_t half{ 1 };

	pmk::Note current_note{ scale };

	if (ImGui::IsKeyDown(ImGuiKey_A)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_S)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_D)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, half);

	if (ImGui::IsKeyDown(ImGuiKey_F)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_G)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_H)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_J)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, half);

	if (ImGui::IsKeyDown(ImGuiKey_K)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_L)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, whole);

	if (ImGui::IsKeyDown(ImGuiKey_Semicolon)) {
		notes.push_back(current_note);
	}
	current_note = NextStep(current_note, half);

	if (ImGui::IsKeyDown(ImGuiKey_Apostrophe)) {
		notes.push_back(current_note);
	}

	return notes;
}
