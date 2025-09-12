#include "rvo_gui_panels.hpp"

#include "rvo_components.hpp"
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace rvo {
	void gui_panel_entities(entt::registry& aRegistry, EditorState& aState) {
		if (!ImGui::Begin("Entities")) {
			ImGui::End();
			return;
		}

		for (auto [entity, gameObject] : aRegistry.view<rvo::GameObject>().each()) {
			ImGui::PushID(static_cast<int>(entity));

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			if (entity == aState.mEditorSelection) flags |= ImGuiTreeNodeFlags_Selected;
			bool opened = ImGui::TreeNodeEx(gameObject.mName.c_str(), flags);

			if (ImGui::IsItemActivated()) {
				aState.mEditorSelection = entity;
			}

			if (opened) {
				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	void gui_panel_properties(entt::handle aHandle) {
		if (aHandle == entt::null) return;

		if (!ImGui::Begin("Properties")) {
			ImGui::End();
			return;
		}

		if (auto* component = aHandle.try_get<rvo::GameObject>()) {
			if (ImGui::CollapsingHeader("GameObject")) {
				ImGui::Checkbox("Enabled", &component->mEnabled);
				ImGui::InputText("Name", &component->mName);
				ImGui::Separator();
				component->mTransform.gui();
			}
		}

		if (auto* component = aHandle.try_get<rvo::MeshRenderer>()) {
			if (ImGui::CollapsingHeader("MeshRenderer")) {
				ImGui::SeparatorText("Fields");

				for (auto& [key, field] : component->mMaterial->mFields) {
					if (auto* value = std::any_cast<glm::vec3>(&field)) {
						ImGui::DragFloat3(key.c_str(), glm::value_ptr(*value));
					}
				}
			}
		}

		if (auto* component = aHandle.try_get<rvo::DirectionalLight>()) {
			if (ImGui::CollapsingHeader("DirectionalLight")) {
				ImGui::ColorEdit3("Color", glm::value_ptr(component->color));
			}
		}

		ImGui::End();
	}
}