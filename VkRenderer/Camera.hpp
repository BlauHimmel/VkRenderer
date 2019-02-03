#pragma once

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#include <glm/glm.hpp>

#include "Namespace.hpp"

NAMESPACE_BEGIN(GLOBAL_NAMESPACE)

class Camera
{
public:
	void Reset();
	void UpdateYaw(float Delta);
	void UpdatePitch(float Delta);
	void UpdateRadius(float Delta);
	void UpdateTarget(float DeltaX, float DeltaY);
	void SetNearFarZ(float NearZ, float FarZ);
	void SetFov(float FovX);
	void SetResolution(float Width, float Height);
	void RetriveData(glm::vec3 & Target, glm::vec3 & Eye, glm::vec3 & Up, glm::vec2 & Fov,float & NearZ, float & FarZ);

	glm::vec3 GetCachedTarget() const;
	glm::vec3 GetCachedUp() const;
	glm::vec3 GetCachedEye() const;

protected:
	void ClampYaw(float & Yaw) const;
	void ClampPitch(float & Pitch) const;
	void ClampRadius(float & Radius) const;

protected:
	glm::vec3 m_Target = glm::vec3(0.0f, 0.0f, 0.0f);

	float m_Yaw = glm::radians(0.0f);
	float m_Pitch = glm::radians(0.0f);
	float m_Radius = 3.0f;

	float m_NearZ = 0.1f;
	float m_FarZ = 100.0f;

	glm::vec2 m_Resolution = glm::vec2(800.0f, 600.0f);;
	glm::vec2 m_Fov = glm::vec2(glm::radians(45.0f), glm::radians(45.0f));

	const float m_YawSpeed = 0.005f;
	const float m_PitchSpeed = 0.005f;
	const float m_RadiusSpeed = 0.2f;
	const float m_TargetSpeed = 0.005f;

	/** Cached data */
	glm::vec3 m_Eye = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 m_Up = glm::vec3(0.0f, 0.0f, 0.0f);
};

NAMESPACE_END