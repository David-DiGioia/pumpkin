#include "input.h"

#include "glm/glm.hpp"

#include "logger.h"
#include "camera_controller.h"
#include "math_util.h"

void ProcessViewportInput(Editor* editor)
{
	float delta{ editor->GetPumpkin()->GetDeltaTime() };
	glm::vec3 move_dir{};

	if (ImGui::IsKeyDown(ImGuiKey_W)) {
		move_dir += glm::vec3{ 0.0f, 0.0f, 1.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_S)) {
		move_dir += glm::vec3{ 0.0f, 0.0f, -1.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_D)) {
		move_dir += glm::vec3{ 1.0f, 0.0f, 0.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_A)) {
		move_dir += glm::vec3{ -1.0f, 0.0f, 0.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_E)) {
		move_dir += glm::vec3{ 0.0f, 1.0f, 0.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_Q)) {
		move_dir += glm::vec3{ 0.0f, -1.0f, 0.0f };
	}

	glm::vec2 mouse_delta{ CastVec2<glm::vec2>(ImGui::GetMouseDragDelta()) };

	CameraController& controller{ editor->GetCameraController() };

	if (move_dir != glm::vec3(0.0f, 0.0f, 0.0f))
	{
		move_dir = glm::normalize(move_dir);
		controller.MoveRelativeToForward(delta * move_dir);
	}

	if (mouse_delta != glm::vec2(0.0f, 0.0f))
	{
		controller.Rotate(mouse_delta.x * 0.01, mouse_delta.y * 0.01);
		ImGui::ResetMouseDragDelta();
	}
}
