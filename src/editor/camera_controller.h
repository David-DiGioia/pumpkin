#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "common_constants.h"
#include "pumpkin.h"

// We make forward negative z-axis so that right can be positive x-axis and up can be positive y-axis,
// while using a right hand coordinate system.
const glm::vec3 CAMERA_LOCAL_FORWARD{ 0.0f, 0.0f, -1.0f };

class CameraController
{
public:
	void Initialize(pmk::Camera* camera);

	// Move the camera relative to the forward direction. If relative_movement == (0, 0, 1),then the camera moves forward.
	void MoveRelativeToForward(const glm::vec3& relative_movement);

	void LookDirection(const glm::vec3& direction);

	void LookAt(const glm::vec3& target);

	// Rotate the camera relative to its current rotation. Revolve around focal point if it's active.
	void Rotate(float phi_delta, float theta_delta);

	glm::vec3 GetForward() const;

private:
	void UpdateCamera();

	glm::vec3 up_{ 0.0f, 1.0f, 0.0f };
	pmk::Camera* camera_{};

	float movement_speed_{ 2.0f };
	float mouse_sensitivity_{ 1.0f };

	glm::vec3 focal_point_{};
	bool focal_point_active_{ false };
	float focal_distance_{ 5.0f };
	float phi_{}; // Horizontal camera revolution.
	float theta_{ PI / 2.0f }; // Vertical camera revolution. 0 is directly above the orbit center.
};
