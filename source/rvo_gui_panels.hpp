#pragma once

#include <entt/entt.hpp>

namespace rvo {
	struct EditorState final {
		entt::entity mEditorSelection = entt::null;
	};

	void gui_panel_entities(entt::registry& aRegistry, EditorState& aState);
	void gui_panel_properties(entt::handle aHandle);
}