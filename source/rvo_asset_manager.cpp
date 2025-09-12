#include "rvo_asset_manager.hpp"

#include "rvo_utility.hpp"

#define STB_INCLUDE_LINE_GLSL
#define STB_INCLUDE_IMPLEMENTATION
#include "stb_include.h"

#include <regex>
#include <spdlog/spdlog.h>

#include <lua.hpp>

#include <cstdint>

namespace rvo {
	namespace {
		std::optional<rvo::ShaderProgram> make_shader(std::string const& aPath) {
			auto optBytes = rvo::read_file_string(aPath);
			if (!optBytes) return std::nullopt;

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
			char* vertSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_VERT\n", "shaders/include", aPath.c_str(), error);
			char* fragSource = stb_include_string(optBytes->c_str(), "#version 460 core\n#define RVO_FRAG\n", "shaders/include", aPath.c_str(), error);

			auto vert = rvo::Shader(GL_VERTEX_SHADER, vertSource);
			auto frag = rvo::Shader(GL_FRAGMENT_SHADER, fragSource);

			std::free(vertSource);
			std::free(fragSource);

			auto program = rvo::ShaderProgram({ vert, frag });
			program.mBackfaceCull = backfaceCulling;
			return program;
		}
	}

	std::shared_ptr<rvo::ShaderProgram> AssetManager::get_shader_program(std::string_view aSource) {
		auto it = mShaderPrograms.find(aSource);
		if (std::shared_ptr<rvo::ShaderProgram> ref; it != mShaderPrograms.end() && (ref = it->second.asset.lock())) return ref;

		auto program = make_shader(std::string(aSource));
		if (!program) return nullptr;

		auto ref = std::make_shared<rvo::ShaderProgram>(std::move(*program));
		mShaderPrograms[std::string(aSource)] = { ref, std::filesystem::last_write_time(aSource) };
		return ref;
	}

	std::shared_ptr<rvo::Mesh> AssetManager::get_mesh(std::string_view aSource) {
		auto it = mMeshes.find(aSource);
		if (std::shared_ptr<rvo::Mesh> ref; it != mMeshes.end() && (ref = it->second.asset.lock())) return ref;

		auto optBytes = rvo::read_file_bytes(aSource);
		if (!optBytes) return nullptr;

		auto ref = std::make_shared<rvo::Mesh>(std::string(aSource).c_str());
		mMeshes[std::string(aSource)] = { ref, std::filesystem::last_write_time(aSource) };
		return ref;
	}

	std::shared_ptr<rvo::Texture> AssetManager::get_texture(std::string_view aSource) {
		auto it = mTextures.find(aSource);
		if (std::shared_ptr<rvo::Texture> ref; it != mTextures.end() && (ref = it->second.asset.lock())) return ref;

		auto ref = std::make_shared<rvo::Texture>(rvo::Texture::make_from_path(std::string(aSource).c_str()));
		mTextures[std::string(aSource)] = { ref, std::filesystem::last_write_time(aSource) };
		return ref;
	}

	std::shared_ptr<rvo::Material> AssetManager::get_material(std::string_view aSource) {
		auto it = mMaterials.find(aSource);
		if (std::shared_ptr<rvo::Material> ref; it != mMaterials.end() && (ref = it->second.asset.lock())) return ref;

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

		mMaterials[std::string(aSource)] = { ref, std::filesystem::last_write_time(aSource) };
		return ref;
	}

	namespace {
		std::uint_fast8_t gCount = 0;
	}

	void AssetManager::update() {
		// Once every 100 frames to reduce frame costs
		if (gCount++ < 100) return;
		gCount = 0;

		// Hot swapping shader support :D
		for (auto it = mShaderPrograms.begin(); it != mShaderPrograms.end();) {
			if (auto ref = it->second.asset.lock()) {
				if (std::filesystem::file_time_type newTime; (newTime = std::filesystem::last_write_time(it->first)) > it->second.lastWriteTime) {
					it->second.lastWriteTime = newTime;

					if (auto shader = make_shader(it->first)) {
						*ref.get() = std::move(*shader);
					}
				}

				++it;
			}
			else {
				it = mShaderPrograms.erase(it);
			}
		}

		for (auto it = mMeshes.begin(); it != mMeshes.end();) {
			if (it->second.asset.expired()) { it = mMeshes.erase(it); }
			else { ++it; }
		}

		for (auto it = mTextures.begin(); it != mTextures.end();) {
			if (it->second.asset.expired()) { it = mTextures.erase(it); }
			else { ++it; }
		}

		for (auto it = mMaterials.begin(); it != mMaterials.end();) {
			if (it->second.asset.expired()) { it = mMaterials.erase(it); }
			else { ++it; }
		}

		
	}
}