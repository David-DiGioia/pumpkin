#include "input.h"

#include <algorithm>
#include "glm/glm.hpp"

#include "editor.h"
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
bool ProcessTransformInput(Editor* editor)
{
	// Transform keyboard shortcuts.
	if (!editor->SelectionEmpty())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
			editor->SetActiveTransformType(TransformType::TRANSLATE);
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
			editor->SetActiveTransformType(TransformType::ROTATE);
		}
		else if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			editor->SetActiveTransformType(TransformType::SCALE);
		}
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
		else if (ImGui::IsKeyPressed(ImGuiKey_X)) {
			editor->SetTransformLock(TransformLockFlags::Y | TransformLockFlags::Z);
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
			editor->SetTransformLock(TransformLockFlags::X | TransformLockFlags::Z);
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
			editor->SetTransformLock(TransformLockFlags::X | TransformLockFlags::Y);
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

void EditorInput::ProcessViewportInput(Editor* editor, const renderer::Extent& viewport_extent)
{
	if (ProcessTransformInput(editor)) {
		return;
	}

	constexpr float zoom_speed{ 0.1f };
	constexpr float movement_speed_scroll_speed{ 0.1f };
	constexpr float rotate_speed{ 0.003f };

	CameraController& controller{ editor->GetCameraController() };
	float delta_time{ editor->GetPumpkin()->GetDeltaTime() };
	glm::vec3 move_dir{};

	// Handle shift modifier.
	if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
	{
		if (ImGui::IsKeyReleased(ImGuiKey_A)) {
			editor->ToggleSelectAll();
		}
	}
	else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_P)) {
			editor->ParentSelectionToActive();
		}
	}
	else if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_P)) {
			editor->ClearSelectionParent();
		}
	}
	else
	{
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
		controller.Focus(editor->GetSelectedNodesAveragePosition(), radius);
	}

	// Revolve / rotate with mouse.
	glm::vec2 mouse_delta{ CastVec2<glm::vec2>(ImGui::GetMouseDragDelta()) };

	if (mouse_delta != glm::vec2(0.0f, 0.0f))
	{
		// We don't multiply by delta time since mouse delta is integer value of number of pixels moved,
		// so often we just get the minimum of 1 pixel, so speed would become mainly determined by framerate which is undesirable.
		controller.Rotate(-mouse_delta.x * rotate_speed, -mouse_delta.y * rotate_speed);
		ImGui::ResetMouseDragDelta();
	}

	// Select object by clicking.
	{
		editor->SetMultiselect(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		glm::vec2 mouse_pos{ GetViewportRelativeMousePos(editor) };
		constexpr float raycast_radius{ 3.0f }; // If mouse moves out of this pixel radius between clicking and releasing, ray is not cast.

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false))
		{
			mouse_down_pos_ = mouse_pos;
			should_cast_ray_on_release_ = true;
		}

		if (glm::distance(mouse_pos, mouse_down_pos_) > raycast_radius) {
			should_cast_ray_on_release_ = false;
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && should_cast_ray_on_release_) {
			editor->CastSelectionRay(mouse_pos, viewport_extent);
		}
	}

	// Zoom / change speed with scroll wheel.
	float wheel{ ImGui::GetIO().MouseWheel };
	if (wheel != 0.0f)
	{
		if (controller.IsFocused())
		{
			float current_dist{ controller.GetFocalDistance() };
			float current_dist_offset{ 0.2f };

			// Proportional to current distance so zooming far in/out still has reasonable speed.
			// Add offset to current distance so maximum zoom in doesn't get stuck at 0 zoom speed.
			float delta_dist{ (-wheel) * zoom_speed * (current_dist + current_dist_offset) };
			controller.SetFocalDistance(std::max(current_dist + delta_dist, 0.0f));
		}
		else
		{
			controller.GetMovementSpeed() += wheel * movement_speed_scroll_speed * controller.GetMovementSpeed();
			controller.GetMovementSpeed() = std::max(controller.GetMovementSpeed(), MINIMUM_MOVEMENT_SPEED);
		}
	}
}

void EditorInput::ProcessTreeViewInput(Editor* editor)
{
	// Handle multiselect.
	editor->SetMultiselect(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
}

void EditorInput::ProcessFileBrowserInput(Editor* editor)
{
	editor->SetMultiselect(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
}
