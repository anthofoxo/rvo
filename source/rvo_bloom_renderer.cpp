#include "rvo_renderer_bloom.hpp"

#include <spdlog/spdlog.h>

namespace rvo {
	void BloomRenderer::init(rvo::AssetManager& aResourceManager) {
		mFramebuffer = rvo::Framebuffer::CreateInfo{};
		GLenum const attachment = GL_COLOR_ATTACHMENT0;
		glNamedFramebufferDrawBuffers(mFramebuffer.handle(), 1, &attachment);

		mProgramUpsample = aResourceManager.get_shader_program("shaders/bloom_upscale.glsl");
		mProgramDownsample = aResourceManager.get_shader_program("shaders/bloom_downscale.glsl");
	}

	void BloomRenderer::render(glm::ivec2 aTargetSize, rvo::Texture& aSourceTexture, float aFilterRadius, float aAspectRatio) {
		if (aTargetSize != mViewportSize) {
			spdlog::debug("Bloom target size changed from {}x{} to {}x{}. Recreating mipchain now", mViewportSize.x, mViewportSize.y, aTargetSize.x, aTargetSize.y);
			mViewportSize = aTargetSize;
			constexpr auto numMips = 8;

			// Create new mipchain
			mMipChain = { {
				.levels = numMips,
				.internalFormat = GL_R11F_G11F_B10F,
				.width = mViewportSize.x / 2,
				.height = mViewportSize.y / 2,
				.minFilter = GL_LINEAR,
				.magFilter = GL_LINEAR,
				.wrap = GL_CLAMP_TO_EDGE,
				.anisotropy = 1.0f,
			} };

			// Calculate mip sizes
			mMipSizes.clear();
			mMipSizes.reserve(numMips);

			glm::ivec2 mipSize = mViewportSize / 2;

			for (auto level = 0; level < numMips; ++level) {
				mMipSizes.emplace_back(mipSize.x, mipSize.y);
				mipSize = glm::max(glm::ivec2(1), mipSize / 2);
			}
		}

		mFramebuffer.bind();
		glDisable(GL_BLEND);
		mMipChain.bind(0);
		mProgramDownsample->bind();

		// Downsample

		// First pass only: Use aSourceTexture as the input
		{
			aSourceTexture.bind(0);
			int const level = 0;

			auto const& srcSize = mViewportSize;
			auto const& dstSize = mMipSizes[level];
			mProgramDownsample->push_2f("srcResolution", srcSize);
			glNamedFramebufferTexture(mFramebuffer.handle(), GL_COLOR_ATTACHMENT0, mMipChain.handle(), level);

			mProgramDownsample->push_1i("uBasePass", 1);
			glViewport(0, 0, dstSize.x, dstSize.y);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glTextureBarrier();
			mProgramDownsample->push_1i("uBasePass", 0);
		}

		// Bind once since texture is the input for all rest of mip chains
		mMipChain.bind(0);

		for (int level = 1; level < mMipSizes.size(); ++level) {
			auto const& srcSize = mMipSizes[level - 1];
			auto const& dstSize = mMipSizes[level];
			mProgramDownsample->push_2f("srcResolution", srcSize);

			glTextureParameteri(mMipChain.handle(), GL_TEXTURE_BASE_LEVEL, level - 1);
			glTextureParameteri(mMipChain.handle(), GL_TEXTURE_MAX_LEVEL, level - 1);
			glNamedFramebufferTexture(mFramebuffer.handle(), GL_COLOR_ATTACHMENT0, mMipChain.handle(), level);

			glViewport(0, 0, dstSize.x, dstSize.y);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glTextureBarrier();
		}

		// Upsample
		mProgramUpsample->bind();
		mProgramUpsample->push_1f("filterRadius", aFilterRadius);
		mProgramUpsample->push_1f("aspectRatio", aAspectRatio);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glBlendEquation(GL_FUNC_ADD);

		// Start at highest mip and work our way back to the base mip
		for (int level = mMipSizes.size() - 1; level > 0; --level) {
			const auto& srcMipSize = mMipSizes[level];
			const auto& dstMipSize = mMipSizes[level - 1];

			// Read from level, write into level-1
			glTextureParameteri(mMipChain.handle(), GL_TEXTURE_BASE_LEVEL, level);
			glTextureParameteri(mMipChain.handle(), GL_TEXTURE_MAX_LEVEL, level);
			glNamedFramebufferTexture(mFramebuffer.handle(), GL_COLOR_ATTACHMENT0, mMipChain.handle(), level - 1);

			glViewport(0, 0, dstMipSize.x, dstMipSize.y);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glTextureBarrier();
		}

		// When completed, when we blend this when compositing, ensure only level 0 is used
		glTextureParameteri(mMipChain.handle(), GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(mMipChain.handle(), GL_TEXTURE_MAX_LEVEL, 1000);

		// Restore default states
		glDisable(GL_BLEND);
	}
}