#pragma once

#include "rvo_gfx.hpp"
#include "rvo_asset_manager.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <memory>

namespace rvo {
	class BloomRenderer final {
	public:
		void init(rvo::AssetManager& aResourceManager);
		void render(glm::ivec2 aTargetSize, rvo::Texture& aSourceTexture, float aFilterRadius, float aAspectRatio);

		rvo::Texture const& mip_chain() const { return mMipChain; }

	private:
		rvo::Texture mMipChain;
		glm::ivec2 mViewportSize{}; // Default init to 0x0
		std::vector<glm::ivec2> mMipSizes;
		rvo::Framebuffer mFramebuffer;
		std::shared_ptr<rvo::ShaderProgram> mProgramUpsample;
		std::shared_ptr<rvo::ShaderProgram> mProgramDownsample;
	};
}