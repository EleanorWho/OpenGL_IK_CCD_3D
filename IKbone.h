#pragma once

/* Container for bone data */

#include <vector>
#include <list>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <iostream>

class IKJoint {
public:
    glm::vec3 position; // global position
    glm::quat localRotation; // Joint's local rotation relative to its parent
    glm::quat globalRotation; // Joint's global rotation in the chain
    float boneLength;  // distance to the next joint

    IKJoint(const glm::vec3& pos, float length = 0.5f)
        : position(pos), boneLength(length), localRotation(glm::quat(1.0, 0.0, 0.0, 0.0)), globalRotation(glm::quat(1.0, 0.0, 0.0, 0.0)) {
    }
};

class IKChain {
public:
    std::vector<IKJoint> joints;

    void addJoint(const IKJoint& joint) {
        joints.push_back(joint);
        // id the joint is not the root joint, set boneLength to the next joint
        if (joints.size() > 1) {
            joints[joints.size() - 2].boneLength = glm::distance(joints[joints.size() - 2].position, joint.position);
        }
    }
};

class IKClass {
public:
    IKChain chain;
    glm::vec3 target;
    int maxIterations;
    float threshold; // threshold distance between endEffector and the targetPos

    IKClass(int maxIter = 30, float thresh = 0.0001f) : maxIterations(maxIter), threshold(thresh) {}

    void applyCCD() {
        for (int iter = 0; iter < maxIterations; ++iter) {
            bool updated = false;
            for (int i = chain.joints.size() - 4; i >= 0; --i) { // Start at second to last joint
                auto& joint = chain.joints[i];
                auto& lastJoint = chain.joints.back();
                glm::vec3 endEffector = lastJoint.position + lastJoint.globalRotation * glm::vec3(lastJoint.boneLength, 0.0, 0.0);

                glm::vec3 toTarget = glm::normalize(target - joint.position);
                glm::vec3 toEndEffector = glm::normalize(endEffector - joint.position);

                float cosTheta = glm::dot(toTarget, toEndEffector);
                float sinTheta = glm::length(glm::cross(toEndEffector, toTarget));

                if (cosTheta < 0.999) { // Ensures there is a need to rotate
                    glm::vec3 rotationAxis = glm::cross(toEndEffector, toTarget);
                    if (sinTheta > glm::epsilon<float>()) { // Avoid invalid rotation
                        rotationAxis = glm::normalize(rotationAxis);
                        float angle = atan2(sinTheta, cosTheta); // Angle calculation

                        //std::cout << "Rotation axis: " << rotationAxis.x << ", " << rotationAxis.y << ", " << rotationAxis.z << "\n";
                        //std::cout << "Rotation angle (radians): " << angle << "\n";
                        //std::cout << "Rotation angle (degrees): " << glm::degrees(angle) << "\n";

                        // Create quaternion for this rotation
                        glm::quat deltaRotation = glm::angleAxis(angle, rotationAxis); // angle is already in radians
                        joint.localRotation = glm::normalize(deltaRotation * joint.localRotation);

                        // Update the global rotation of the current joint
                        if (i == 0) {
                            joint.globalRotation = joint.localRotation; // Root joint has no parent
                        }
                        else {
                            joint.globalRotation = chain.joints[i - 1].globalRotation * joint.localRotation;
                        }

                        //std::cout << "Joint " << i << " local rotation (after update): " << joint.localRotation.x << ", " << joint.localRotation.y << ", " << joint.localRotation.z << "\n";
                        //std::cout << "Joint " << i << " global rotation (after update): " << joint.globalRotation.x << ", " << joint.globalRotation.y << ", " << joint.globalRotation.z << "\n";

                        // Recursively update all child joints' global rotations and positions
                        glm::quat cumulativeRotation = joint.globalRotation;

                        for (int j = i + 1; j < chain.joints.size(); ++j) {
                            // Calculate the position of the next joint by rotating the direction to the next joint
                            glm::vec3 nextJointDirection = cumulativeRotation * glm::vec3(chain.joints[j - 1].boneLength, 0.0, 0.0);
                            chain.joints[j].position = chain.joints[j - 1].position + nextJointDirection;
                            chain.joints[j].localRotation = deltaRotation * chain.joints[j].localRotation;
                            chain.joints[j].globalRotation = cumulativeRotation * chain.joints[j].localRotation;
                            cumulativeRotation = chain.joints[j].globalRotation;

                            //std::cout << "Child joint " << j << " position (after update): " << chain.joints[j].position.x << ", " << chain.joints[j].position.y << ", " << chain.joints[j].position.z << "\n";
                            //std::cout << "Child joint " << j << " global rotation (after update): " << chain.joints[j].globalRotation.x << ", " << chain.joints[j].globalRotation.y << ", " << chain.joints[j].globalRotation.z << "\n";
                        }

                        updated = true; // Flag that we updated at least one joint
                    }
                }
            }
            
            // Check if we are close enough to the target to terminate
            if (glm::distance(chain.joints.back().position, target) < threshold) {
                break; // Exit if we've reached the target within the threshold
            }

            if (!updated) {
                break; // Exit if no joints were updated
            }
        }
        //glm::vec3 endEffector = chain.joints.back().position + chain.joints.back().globalRotation * glm::vec3(chain.joints.back().boneLength, 0.0, 0.0);
        //std::cout << "endEffector position is: " << endEffector.x << ", " << endEffector.y << ", " << endEffector.z << std::endl;
    }

    glm::mat4 getRootTransform() const {
        if (chain.joints.empty()) return glm::mat4(1.0f);
        const auto& root = chain.joints.front();
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), root.position);
        glm::mat4 rotation = glm::toMat4(root.globalRotation); // Convert quaternion to rotation matrix
        return translation * rotation; // Combine translation and rotation
    }

    void setTarget(glm::vec3 newTarget) {
        target = newTarget;
    }
};