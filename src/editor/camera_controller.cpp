#include "camera_controller.h"

#include <algorithm>
#include "glm/gtx/euler_angles.hpp"

void CameraController::Initialize(pmk::Camera* camera)
{
	camera_ = camera;
}

void CameraController::MoveRelativeToForward(const glm::vec3& relative_movement)
{
	glm::vec3 global_movement{ camera_->rotation * relative_movement };
	camera_->position += global_movement;
}

void CameraController::LookDirection(const glm::vec3& direction)
{
	glm::vec3 dir{ glm::normalize(direction) };

	glm::vec3 x{ glm::normalize(glm::cross(dir, up_)) };
	glm::vec3 z{ -dir };
	glm::vec3 y{ glm::normalize(glm::cross(z, x)) };
	glm::mat3 coordinate_system{ x, y, z };

	camera_->rotation = glm::quat_cast(coordinate_system);
}

void CameraController::LookAt(const glm::vec3& target)
{
	glm::vec3 dir{ glm::normalize(target - camera_->position) };
	LookDirection(dir);
}

void CameraController::Rotate(float phi_delta, float theta_delta)
{
	phi_ += phi_delta;
	theta_ += theta_delta;
	constexpr float epsilon{ 0.00001f };
	theta_ = std::clamp(theta_, epsilon, PI - epsilon);
	UpdateCamera();
}

glm::vec3 CameraController::GetForward() const
{
	return camera_->rotation * CAMERA_LOCAL_FORWARD;
}

void CameraController::UpdateCamera()
{
	// This is matrix multiplication Y * X.
	// This is analogous to gimbal with X as the inner ring and Y as the outer ring.
	// So first we rotate the vector from pointing up to oblique, in a lever pulling motion.
	// Then we rotate about the y-axis in a stirring motion.
	glm::mat4 rotation{ glm::eulerAngleYX(phi_, theta_) };

	// Initialize as direction when phi_ == theta_ == 0. 
	glm::vec3 focal_point_to_camera{ 0.0f, 1.0f, 0.0f };
	focal_point_to_camera = glm::mat3(rotation) * focal_point_to_camera;
	LookDirection(-focal_point_to_camera);

	camera_->position = focal_point_ + focal_distance_ * focal_point_to_camera;
}
