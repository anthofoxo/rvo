#include "rvo_asset_manager.hpp"

#include "rvo_utility.hpp"

namespace rvo {
	std::shared_ptr<rvo::ShaderProgram> AssetManager::get_shader_program(std::string const& aSource) {
		auto it = mShaderPrograms.find(aSource);
		if (it != mShaderPrograms.end()) return it->second;

		auto optBytes = rvo::read_file_bytes(aSource);
		if (!optBytes) return nullptr;

		auto vert = rvo::Shader(GL_VERTEX_SHADER, *optBytes);
		auto frag = rvo::Shader(GL_FRAGMENT_SHADER, *optBytes);

		auto ref = std::make_shared<rvo::ShaderProgram>(rvo::ShaderProgram({ vert, frag }));
		mShaderPrograms[aSource] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Mesh> AssetManager::get_mesh(std::string const& aSource) {
		auto it = mMeshes.find(aSource);
		if (it != mMeshes.end()) return it->second;

		auto optBytes = rvo::read_file_bytes(aSource);
		if (!optBytes) return nullptr;

		auto ref = std::make_shared<rvo::Mesh>(aSource.c_str());
		mMeshes[aSource] = ref;
		return ref;
	}

	std::shared_ptr<rvo::Texture> AssetManager::get_texture(std::string const& aSource) {
		auto it = mTextures.find(aSource);
		if (it != mTextures.end()) return it->second;

		auto ref = std::make_shared<rvo::Texture>(rvo::Texture::make_from_path(aSource.c_str()));
		mTextures[aSource] = ref;
		return ref;
	}
}