#include "input.h"

#include <algorithm>
#include "glm/glm.hpp"

#include "logger.h"
#include "camera_controller.h"
#include "math_util.h"

// Converts a vector with units of pixels to a vector with units of viewport height.
static glm::vec2 PixelToViewportUnits(Editor* editor, const glm::vec2& pixel_vec)
{
	auto& viewport_size{ editor->GetGui().GetViewportExtent() };
	return glm::vec2{ pixel_vec.x / viewport_size.height, pixel_vec.y / viewport_size.height };
}

// Get the mouse position relative to the top left corner of the 3D viewport, in units of pixels.
static glm::vec2 GetViewportRelativeMousePos(Editor* editor)
{
	uint32_t header_size{ editor->GetGui().GetViewportWindowExtent().height - editor->GetGui().GetViewportExtent().height };
	glm::vec2 window_pos{ CastVec2<glm::vec2>(ImGui::GetWindowPos()) };
	glm::vec2 viewport_pos{ window_pos.x, window_pos.y + header_size };

	return CastVec2<glm::vec2>(ImGui::GetMousePos()) - viewport_pos;
}

// Returns true if editor is actively in a transform mode (like translating a node with mouse input).
static bool ProcessTransformInput(Editor* editor)
{
	// Transform keyboard shortcuts.
	if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !editor->SelectionEmpty()) {
		editor->SetActiveTransformType(TransformType::TRANSLATE);
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !editor->SelectionEmpty()) {
		editor->SetActiveTransformType(TransformType::ROTATE);
	}

	// Handle transform input separately since it overrides other input.
	if (editor->GetActiveTransformType() != TransformType::NONE)
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			editor->ApplyTransformInput();
		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			editor->CancelTransformInput();
		}
		else
		{
			glm::vec2 relative_mouse_pos{ GetViewportRelativeMousePos(editor) };
			editor->ProcessTransformInput(PixelToViewportUnits(editor, relative_mouse_pos));
		}
		return true;
	}

	return false;
}

void ProcessViewportInput(Editor* editor)
{
	if (ProcessTransformInput(editor)) {
		return;
	}

	constexpr float zoom_speed{ 0.1f };
	constexpr float rotate_speed{ 2.0f };

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

	// Focus on target on key press.
	EditorNode* active_selection{ editor->GetActiveSelectionNode() };
	if (ImGui::IsKeyDown(ImGuiKey_KeypadDecimal) && active_selection)
	{
		// TODO: Don't hardcode radius. This should be calculated from node's bounding box.
		float radius{ 5.0f };
		controller.Focus(active_selection->node->position, radius);
	}

	// Revolve / rotate with mouse.
	glm::vec2 mouse_delta{ CastVec2<glm::vec2>(ImGui::GetMouseDragDelta()) };

	if (mouse_delta != glm::vec2(0.0f, 0.0f))
	{
		controller.Rotate(-mouse_delta.x * rotate_speed * delta_time, -mouse_delta.y * rotate_speed * delta_time);
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
		float delta_dist{ (-wheel) * zoom_speed * (current_dist + current_dist_offset) };
		controller.SetFocalDistance(std::max(current_dist + delta_dist, 0.0f));
	}
}

void ProcessTreeViewInput(Editor* editor)
{
	// Handle multiselect.
	editor->SetMultiselect(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
}
