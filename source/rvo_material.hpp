#pragma once

#include "rvo_gfx.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <any>

namespace rvo {
	struct Material final {
		std::shared_ptr<rvo::ShaderProgram> mShaderProgram;
		std::shared_ptr<rvo::Texture> mTexture;
		std::unordered_map<std::string, std::any> mFields;
	};
}