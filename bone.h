#pragma once
#pragma once

/* Container for bone data */

#include <vector>
#include <assimp/scene.h>
#include <list>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include "assimp_glm_helpers.h"

struct KeyPosition
{
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation
{
	glm::quat orientation;
	float timeStamp;
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp;
};

class Bone
{
public:
	Bone(const std::string& name, int ID, const aiNodeAnim* channel)
		:
		m_Name(name),
		m_ID(ID),
		m_LocalTransform(1.0f),
		m_GlobalTransform(1.0f)

	{
		if (channel == nullptr) {
			throw std::invalid_argument("channel must not be null");
		}

		m_NumPositions = channel->mNumPositionKeys;

		for (int positionIndex = 0; positionIndex < m_NumPositions; ++positionIndex)
		{
			aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
			float timeStamp = channel->mPositionKeys[positionIndex].mTime;
			KeyPosition data;
			data.position = AssimpGLMHelpers::GetGLMVec(aiPosition);
			data.timeStamp = timeStamp;
			m_Positions.push_back(data);
		}

		m_NumRotations = channel->mNumRotationKeys;
		for (int rotationIndex = 0; rotationIndex < m_NumRotations; ++rotationIndex)
		{
			aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
			float timeStamp = channel->mRotationKeys[rotationIndex].mTime;
			KeyRotation data;
			data.orientation = AssimpGLMHelpers::GetGLMQuat(aiOrientation);
			data.timeStamp = timeStamp;
			m_Rotations.push_back(data);
		}

		m_NumScalings = channel->mNumScalingKeys;
		for (int keyIndex = 0; keyIndex < m_NumScalings; ++keyIndex)
		{
			aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
			float timeStamp = channel->mScalingKeys[keyIndex].mTime;
			KeyScale data;
			data.scale = AssimpGLMHelpers::GetGLMVec(scale);
			data.timeStamp = timeStamp;
			m_Scales.push_back(data);
		}
	}

	void Update(float animationTime)
	{
		glm::mat4 translation = InterpolatePosition(animationTime);
		glm::mat4 rotation = InterpolateRotation(animationTime);
		glm::mat4 scale = InterpolateScaling(animationTime);
		m_LocalTransform = translation * rotation * scale;
	}
	glm::mat4 GetLocalTransform() const { return m_LocalTransform; }
	std::string GetBoneName() const { return m_Name; }
	int GetBoneID() { return m_ID; }

	std::vector<KeyPosition> GetBonePosition() { return m_Positions; }


	int GetPositionIndex(float animationTime)
	{
		for (int index = 0; index < m_NumPositions - 1; ++index)
		{
			if (animationTime < m_Positions[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	int GetRotationIndex(float animationTime)
	{
		for (int index = 0; index < m_NumRotations - 1; ++index)
		{
			if (animationTime < m_Rotations[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	int GetScaleIndex(float animationTime)
	{
		for (int index = 0; index < m_NumScalings - 1; ++index)
		{
			if (animationTime < m_Scales[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	// 添加一个函数来根据目标位置更新骨骼旋转
	void UpdateRotationTowardsTarget(const glm::vec3& targetPosition) {
		if (m_Parent) { // 确保有父骨骼
			glm::vec3 boneDir = glm::normalize(glm::vec3(m_GlobalTransform[3]) - glm::vec3(m_Parent->m_GlobalTransform[3]));
			glm::vec3 targetDir = glm::normalize(targetPosition - glm::vec3(m_Parent->m_GlobalTransform[3]));
			float dot = glm::dot(boneDir, targetDir);
			float angle = acos(dot);
			// 这里简化处理，实际应用中需要考虑旋转轴和旋转方向
			glm::quat rotation = glm::angleAxis(angle, glm::vec3(0, 0, 1)); // 假设绕Z轴旋转
			SetLocalRotation(rotation);
		}
	}

	// 设置骨骼的局部旋转
	void SetLocalRotation(const glm::quat& rotation) {
		// 正确处理先前的平移和缩放
		glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), GetPosition());
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), GetScale());
		glm::mat4 rotationMat = glm::toMat4(rotation);
		m_LocalTransform = translationMat * rotationMat * scaleMat;
	}

	// 更新骨骼的全局变换矩阵
	void UpdateGlobalTransform(const glm::mat4& parentTransform) {
		m_GlobalTransform = parentTransform * m_LocalTransform;
		// 递归更新所有子骨骼的全局变换
		for (Bone* child : m_Children) {
			child->UpdateGlobalTransform(m_GlobalTransform);
		}
	}

	// 获取骨骼的局部位置
	glm::vec3 GetPosition() {
		// 假设位置是局部变换矩阵的第四列
		return glm::vec3(m_LocalTransform[3]);
	}

	// 获取骨骼的局部缩放
	glm::vec3 GetScale() const {
		// 假设缩放可以从局部变换矩阵的对角线元素中提取
		return glm::vec3(m_LocalTransform[0][0], m_LocalTransform[1][1], m_LocalTransform[2][2]);
	}

	// 添加子骨骼
	void AddChild(Bone* child) {
		m_Children.push_back(child);
		child->m_Parent = this; // 设置父骨骼
	}

	// 获取父骨骼的全局变换矩阵
	glm::mat4 GetParentGlobalTransform() const {
		if (m_Parent) {
			return m_Parent->m_GlobalTransform;
		}
		return glm::mat4(1.0f); // 如果没有父骨骼，返回单位矩阵
	}

	glm::mat4 GetGlobalTransform() const {
		if (m_Parent == nullptr) {
			return m_LocalTransform;
		}
		else {
			return m_Parent->GetGlobalTransform() * m_LocalTransform;
		}
	}

	std::vector<Bone*> m_Children; // 子骨骼
	Bone* m_Parent;

private:

	float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
	{
		float scaleFactor = 0.0f;
		float midWayLength = animationTime - lastTimeStamp;
		float framesDiff = nextTimeStamp - lastTimeStamp;
		scaleFactor = midWayLength / framesDiff;
		return scaleFactor;
	}

	glm::mat4 InterpolatePosition(float animationTime)
	{
		if (1 == m_NumPositions)
			return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

		int p0Index = GetPositionIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Positions[p0Index].timeStamp,
			m_Positions[p1Index].timeStamp, animationTime);
		glm::vec3 finalPosition = glm::mix(m_Positions[p0Index].position, m_Positions[p1Index].position
			, scaleFactor);
		return glm::translate(glm::mat4(1.0f), finalPosition);
	}

	glm::mat4 InterpolateRotation(float animationTime)
	{
		if (1 == m_NumRotations)
		{
			auto rotation = glm::normalize(m_Rotations[0].orientation);
			return glm::toMat4(rotation);
		}

		int p0Index = GetRotationIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Rotations[p0Index].timeStamp,
			m_Rotations[p1Index].timeStamp, animationTime);
		glm::quat finalRotation = glm::slerp(m_Rotations[p0Index].orientation, m_Rotations[p1Index].orientation
			, scaleFactor);
		finalRotation = glm::normalize(finalRotation);
		return glm::toMat4(finalRotation);

	}

	glm::mat4 InterpolateScaling(float animationTime)
	{
		if (1 == m_NumScalings)
			return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

		int p0Index = GetScaleIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Scales[p0Index].timeStamp,
			m_Scales[p1Index].timeStamp, animationTime);
		glm::vec3 finalScale = glm::mix(m_Scales[p0Index].scale, m_Scales[p1Index].scale
			, scaleFactor);
		return glm::scale(glm::mat4(1.0f), finalScale);
	}

	std::vector<KeyPosition> m_Positions;
	std::vector<KeyRotation> m_Rotations;
	std::vector<KeyScale> m_Scales;
	int m_NumPositions;
	int m_NumRotations;
	int m_NumScalings;

	glm::mat4 m_LocalTransform;
	std::string m_Name;
	int m_ID;

	// 成员变量
	glm::mat4 m_GlobalTransform;

};
