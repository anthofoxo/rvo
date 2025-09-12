#pragma once

#include <entt/entt.hpp>

#include "rvo_components.hpp"
#include "rvo_renderer_bloom.hpp"

namespace rvo {
	struct EngineShaderData final {
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		alignas(16) glm::vec3 sunDirection;
		alignas(4)  float farPlane;
		alignas(4)  float time;
	};

	struct GBuffers final {
		glm::ivec2 mSize{};
		rvo::Texture mFboAlbedo;
		rvo::Texture mFboNormal;
		rvo::Texture mFboDepth;
		rvo::Framebuffer mFbo;

		rvo::Framebuffer mFboMixed;
		rvo::Texture mFboMixedColor;

		rvo::Texture mFboFinalColor;
		rvo::Framebuffer mFboFinal;

		void resize(glm::ivec2 const& aTargetSize);
	};

	struct InstanceRendererData {
		rvo::Buffer mBuffer;
		std::size_t mInstancesPerDraw;

		void init(std::size_t aInstancesPerDraw);

		std::span<glm::mat4> begin(GLsizeiptr numInstances);

		void finish();
	};

	struct Renderer final {
		void init(rvo::AssetManager& aAssetManager);
		void render_scene(glm::ivec2 aTargetSize, entt::registry& aRegistry, Transform const& aTransform, Camera aCamera, float aCurrentTime);

		rvo::Texture& get_output_texture();

		rvo::Buffer mEngineShaderBuffer;
		EngineShaderData mEngineShaderData;
		glm::ivec2 mTargetSize;
		GBuffers mGBuffers;
		InstanceRendererData mInstanceRendererData;
		rvo::BloomRenderer bloomRenderer;

		float mBloomBlend = 0.02f;
		float mFilterRadius = 0.003f;
		int mNumBatches;
		int mNumEntities;
		bool mWireframe = false;

		std::shared_ptr<rvo::ShaderProgram> mShaderProgramFinal;
		std::shared_ptr<rvo::ShaderProgram> mShaderProgramComposite;
	};
}