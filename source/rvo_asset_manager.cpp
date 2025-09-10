#include "rvo_asset_manager.hpp"

#include "rvo_utility.hpp"

#define STB_INCLUDE_LINE_GLSL
#define STB_INCLUDE_IMPLEMENTATION
#include "stb_include.h"

#include <regex>
#include <spdlog/spdlog.h>

#include <lua.hpp>

namespace rvo {
	std::shared_ptr<rvo::ShaderProgram> AssetManager::get_shader_program(std::string_view aSource) {
		auto it = mShaderPrograms.find(aSource);
		if (it != mShaderPrograms.end()) return it->second;

		auto optBytes = rvo::read_file_string(aSource);
		if (!optBytes) return nullptr;

		bool backfaceCulling = true;

		{
			std::regex pattern(R"(#\s*pragma\s+RVO_NO_BACKFACE_CULL)", std::regex::icase);

			auto it = std::sregex_iterator(optBytes->begin(), optBytes->end(), pattern);
			auto end = std::sregex_iterator();
			std::size_t matchCount = std::distance(it, end);

			*optBytes = std::regex_replace(*optBytes, pattern, "// $&");

			if (matchCount > 0) {
				backfaceCulling = false;
			}
		}

		char error[256];
		char* vertSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_VERT\n", "shaders/include", std::string(aSource).c_str(), error);
		char* fragSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_FRAG\n", "shaders/include", std::string(aSource).c_str(), error);

		auto vert = rvo::Shader(GL_VERTEX_SHADER, vertSource);
		auto frag = rvo::Shader(GL_FRAGMENT_SHADER, fragSource);

		free(vertSource);
		free(fragSource);

		auto ref = std::make_shared<rvo::ShaderProgram>(rvo::ShaderProgram({ vert, frag }));

		ref->mBackfaceCull = backfaceCulling;

		mShaderPrograms[std::string(aSource)] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Mesh> AssetManager::get_mesh(std::string_view aSource) {
		auto it = mMeshes.find(aSource);
		if (it != mMeshes.end()) return it->second;

		auto optBytes = rvo::read_file_bytes(aSource);
		if (!optBytes) return nullptr;

		auto ref = std::make_shared<rvo::Mesh>(std::string(aSource).c_str());
		mMeshes[std::string(aSource)] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Texture> AssetManager::get_texture(std::string_view aSource) {
		auto it = mTextures.find(aSource);
		if (it != mTextures.end()) return it->second;

		auto ref = std::make_shared<rvo::Texture>(rvo::Texture::make_from_path(std::string(aSource).c_str()));
		mTextures[std::string(aSource)] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Material> AssetManager::get_material(std::string_view aSource) {
		auto it = mMaterials.find(aSource);
		if (it != mMaterials.end()) return it->second;

		lua_State* L = luaL_newstate();
		luaL_openlibs(L);

		if (luaL_dofile(L, std::string(aSource).c_str()) != LUA_OK) {
			spdlog::warn("Failed to load material `{}`: {}", aSource, lua_tostring(L, -1));
			lua_pop(L, 1);
			return nullptr;
		}

		auto ref = std::make_shared<rvo::Material>();

		if (lua_getfield(L, -1, "shaderProgram") == LUA_TSTRING) {
			ref->mShaderProgram = get_shader_program(lua_tostring(L, -1));
		}
		lua_pop(L, 1);

		if (lua_getfield(L, -1, "texture") == LUA_TSTRING) {
			ref->mTexture = get_texture(lua_tostring(L, -1));
		}
		lua_pop(L, 1);

		if (lua_getfield(L, -1, "fields") == LUA_TTABLE) {
			lua_pushnil(L);

			while (lua_next(L, -2)) {
				lua_pushvalue(L, -2);
				std::string key = lua_tostring(L, -1);

				// glm::vec3
				if (lua_istable(L, -2) && lua_rawlen(L, -2) == 3) {
					glm::vec3 value;

					for (int i = 0; i < 3; ++i) {
						lua_rawgeti(L, -2, i + 1);
						value[i] = static_cast<float>(lua_tonumber(L, -1));
						lua_pop(L, 1);
					}

					ref->mFields[std::move(key)] = value;
				}
				else {
					spdlog::warn("Unsupported field: {}", key);
				}

				lua_pop(L, 2);
			}
		}
		lua_pop(L, 1);

		lua_close(L);

		mMaterials[std::string(aSource)] = ref;
		return ref;
	}

	
}