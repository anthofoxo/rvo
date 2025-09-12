#pragma once

#include "rvo_gfx.hpp"
#include "rvo_material.hpp"
#include "rvo_transform.hpp"

#include <string>
#include <memory>

namespace rvo {
	struct GameObject final {
		bool mEnabled = true;
		rvo::Transform mTransform;
		std::string mName = "Unnamed";
	};

	struct MeshRenderer final {
		std::shared_ptr<rvo::Mesh> mMesh;
		std::shared_ptr<rvo::Material> mMaterial;
	};

	struct DirectionalLight final {
		glm::vec3 color;
	};
}