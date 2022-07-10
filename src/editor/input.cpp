#include "input.h"

#include <algorithm>
#include "glm/glm.hpp"

#include "logger.h"
#include "camera_controller.h"
#include "math_util.h"

void ProcessViewportInput(Editor* editor)
{
	constexpr float mouse_wheel_multiplier{ 0.1f };

	CameraController& controller{ editor->GetCameraController() };
	float delta_time{ editor->GetPumpkin()->GetDeltaTime() };
	glm::vec3 move_dir{};

	// Keyboard movement.
	if (ImGui::IsKeyDown(ImGuiKey_W)) {
		move_dir += glm::vec3{ 0.0f, 0.0f, -1.0f };
	}
	if (ImGui::IsKeyDown(ImGuiKey_S)) {
		move_dir += glm::vec3{ 0.0f, 0.0f, 1.0f };
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

	if (move_dir != glm::vec3(0.0f, 0.0f, 0.0f))
	{
		move_dir = glm::normalize(move_dir);
		controller.MoveRelativeToForward(delta_time * move_dir);
	}

	// Revolve / rotate with mouse.
	glm::vec2 mouse_delta{ CastVec2<glm::vec2>(ImGui::GetMouseDragDelta()) };

	if (mouse_delta != glm::vec2(0.0f, 0.0f))
	{
		controller.Rotate(-mouse_delta.x * delta_time, -mouse_delta.y * delta_time);
		ImGui::ResetMouseDragDelta();
	}

	// Zoom with scroll wheel.
	float wheel{ ImGui::GetIO().MouseWheel };
	if (wheel != 0.0f && controller.IsFocused())
	{
		float current_dist{ controller.GetFocalDistance() };
		float current_dist_offset{ 0.2f };

		// Proportional to current distance so zooming far in/out still has reasonable speed.
		// Add offset to current distance so maximum zoom in doesn't get stuck at 0 zoom speed.
		float delta_dist{ (-wheel) * mouse_wheel_multiplier * (current_dist + current_dist_offset)};
		controller.SetFocalDistance(std::max(current_dist + delta_dist, 0.0f));
	}
}
