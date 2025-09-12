#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace rvo {
	struct Transform final {
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		glm::quat orientation = glm::identity<glm::quat>();
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

		glm::mat4 get() const;
		void set(glm::mat4 const& matrix);
		void translate(glm::vec3 const& aValue);
		void reset();
		void gui();

		glm::vec3 right() const;
		glm::vec3 up() const;
		glm::vec3 forward() const;
	};
}