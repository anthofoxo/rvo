#include "rvo_transform.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace rvo {
	glm::mat4 Transform::get() const {
		constexpr glm::vec3 skew = { 0.0f, 0.0f, 0.0f };
		constexpr glm::vec4 perspective = { 0.0f, 0.0f, 0.0f, 1.0f };
		return glm::recompose(scale, orientation, position, skew, perspective);
	}

	void Transform::set(glm::mat4 const& matrix) {
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(matrix, scale, orientation, position, skew, perspective);
	}

	void Transform::translate(glm::vec3 const& aValue) {
		glm::mat4 const result = glm::translate(get(), aValue);
		position = result[3];
	}

	void Transform::reset() { *this = Transform{}; }

	void Transform::gui() {
		glm::vec3 const euler = glm::degrees(glm::eulerAngles(orientation));
		glm::vec3 newEuler = euler;

		ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1f);
		if (ImGui::DragFloat3("Rotation", glm::value_ptr(newEuler))) {
			glm::vec3 const deltaEuler = glm::radians(euler - newEuler);
			orientation = glm::rotate(orientation, deltaEuler.x, { 1.0f, 0.0f, 0.0f });
			orientation = glm::rotate(orientation, deltaEuler.y, { 0.0f, 1.0f, 0.0f });
			orientation = glm::rotate(orientation, deltaEuler.z, { 0.0f, 0.0f, 1.0f });
		}
		ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f);

		if (ImGui::SmallButton("Reset Transform")) { reset(); }

		ImGui::SameLine();

		if (ImGui::SmallButton("Reset Orientation")) { orientation = glm::identity<glm::quat>(); }
	}

	glm::vec3 Transform::right() const { return orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
	glm::vec3 Transform::up() const { return orientation * glm::vec3(0.0f, 1.0f, 0.0f); }
	glm::vec3 Transform::forward() const { return orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
}