#include "stdafx.h"
#include "camera.h"
#include "pipeline.h"
#include "render.h"
#include "settings.h"
#include "directional_light.h"

DirLight::DirLight()
{
	initCascadedShadowMapFBO();
}

void DirLight::Update(const glm::vec3& pos, const glm::vec3& dir)
{
	tPos_ = pos;
	tDir_ = dir;

	view_ = glm::lookAt(tPos_, glm::normalize(Render::GetCamera()->GetPos()), glm::vec3(0.0, 1.0, 0.0));

	// equally spaced cascade ends (may change in future)
	float persNear = Render::GetCamera()->GetNear();
	float persFar = Render::GetCamera()->GetFar();
	cascadeEnds_[0] = persNear;
	cascadeEnds_[1] = 35.f;
	cascadeEnds_[2] = 70.f;
	cascadeEnds_[3] = persFar;
	calcOrthoProjs();
}

void DirLight::initCascadedShadowMapFBO()
{
	// configure depth map FBO
	// -----------------------
	glGenFramebuffers(1, &depthMapFBO_);
	// create depth texture
	glGenTextures(shadowCascades_, &depthMapTexes_[0]);

	//float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	float borderColor[] = { 0, 0, 0, 0 };
	for (unsigned i = 0; i < shadowCascades_; i++)
	{
		glBindTexture(GL_TEXTURE_2D, depthMapTexes_[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadowSize_.x, shadowSize_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	}

	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexes_[0], 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	ASSERT_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
		"Shadow cascade framebuffer incomplete.");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DirLight::bindForWriting(unsigned index)
{
	ASSERT(index < shadowCascades_);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthMapFBO_);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexes_[index], 0);
}

void DirLight::bindForReading()
{
	//switch (shadowCascades_) // fall-through intended
	//{
	//case 3:
	//	glActiveTexture(2);
	//	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[2]);
	//case 2:
	//	glActiveTexture(1);
	//	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[1]);
	//case 1:
	//	glActiveTexture(0);
	//	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[0]);
	//}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[1]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, depthMapTexes_[2]);
}

void DirLight::calcOrthoProjs()
{
	// Get the inverse of the view transform
	glm::mat4 Cam = Render::GetCamera()->GetView();
	//glm::mat4 Cam = glm::lookAt(pos_, Render::GetCamera()->GetDir(), glm::vec3(0, 1, 0));
	glm::mat4 CamInv = glm::inverse(Cam);

	// Get the light space tranform
	glm::mat4 LightM = view_;
	//glm::mat4 LightM = glm::lookAt(glm::vec3(0), dir_, glm::vec3(0, 1, 0));
	//glm::mat4 LightM = glm::lookAt(-pos_, dir_, glm::vec3(0, 1, 0));

	float ar = (float)Settings::Graphics.screenX / (float)Settings::Graphics.screenY;
	float fov = Render::GetCamera()->GetFov(); // degrees
	float tanHalfHFOV = tanf(glm::radians(fov / 2.0f));
	float tanHalfVFOV = tanf(glm::radians((fov * ar) / 2.0f));

	for (unsigned i = 0; i < shadowCascades_; i++)
	{
		float xn = cascadeEnds_[i] * tanHalfHFOV;
		float xf = cascadeEnds_[i + 1] * tanHalfHFOV;
		float yn = cascadeEnds_[i] * tanHalfVFOV;
		float yf = cascadeEnds_[i + 1] * tanHalfVFOV;

		glm::vec4 frustumCorners[8] =
		{
			// near face
			glm::vec4(xn, yn, cascadeEnds_[i], 1.0),
			glm::vec4(-xn, yn, cascadeEnds_[i], 1.0),
			glm::vec4(xn, -yn, cascadeEnds_[i], 1.0),
			glm::vec4(-xn, -yn, cascadeEnds_[i], 1.0),

			// far face
			glm::vec4(xf, yf, cascadeEnds_[i + 1], 1.0),
			glm::vec4(-xf, yf, cascadeEnds_[i + 1], 1.0),
			glm::vec4(xf, -yf, cascadeEnds_[i + 1], 1.0),
			glm::vec4(-xf, -yf, cascadeEnds_[i + 1], 1.0)
		};
		glm::vec4 frustumCornersL[8];

		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::min();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::min();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::min();

		for (unsigned j = 0; j < 8; j++)
		{
			// Transform the frustum coordinate from view to world space
			glm::vec4 vW = CamInv * frustumCorners[j];

			// Transform the frustum coordinate from world to light space
			frustumCornersL[j] = LightM * vW;

			minX = glm::min(minX, frustumCornersL[j].x);
			maxX = glm::max(maxX, frustumCornersL[j].x);
			minY = glm::min(minY, frustumCornersL[j].y);
			maxY = glm::max(maxY, frustumCornersL[j].y);
			minZ = glm::min(minZ, frustumCornersL[j].z);
			maxZ = glm::max(maxZ, frustumCornersL[j].z);
		}

		//shadowOrthoProjInfo_[i].r = maxX;
		//shadowOrthoProjInfo_[i].l = minX;
		//shadowOrthoProjInfo_[i].b = minY;
		//shadowOrthoProjInfo_[i].t = maxY;
		//shadowOrthoProjInfo_[i].f = maxZ;
		//shadowOrthoProjInfo_[i].n = minZ;
		//shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ) * view_;
		//shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, 0.f, 100.f) * LightM;
		//shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ) * glm::lookAt(pos_, Render::GetCamera()->GetPos(), glm::vec3(0, 1, 0));
		//shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ) * Render::GetCamera()->GetView());
		//shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ) * LightM;
		shadowOrthoProjMtxs_[i] = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
	}
}