#include "rvo_renderer.hpp"

namespace rvo {
	void GBuffers::resize(glm::ivec2 const& aTargetSize) {
		if (aTargetSize == mSize) return;

		mSize = aTargetSize;

		mFbo = rvo::Framebuffer::CreateInfo{};

		mFboAlbedo = { {
			.levels = 1,
			.internalFormat = GL_R11F_G11F_B10F,
			.width = mSize.x,
			.height = mSize.y,
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR,
			.wrap = GL_CLAMP_TO_EDGE,
			.anisotropy = 1.0f,
		} };

		mFboNormal = { {
			.levels = 1,
			.internalFormat = GL_RGBA16F,
			.width = mSize.x,
			.height = mSize.y,
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR,
			.wrap = GL_CLAMP_TO_EDGE,
			.anisotropy = 1.0f,
		} };

		mFboDepth = { {
			.levels = 1,
			.internalFormat = GL_DEPTH_COMPONENT32F,
			.width = mSize.x,
			.height = mSize.y,
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR,
			.wrap = GL_CLAMP_TO_EDGE,
			.anisotropy = 1.0f,
		} };

		glNamedFramebufferTexture(mFbo.handle(), GL_COLOR_ATTACHMENT0, mFboAlbedo.handle(), 0);
		glNamedFramebufferTexture(mFbo.handle(), GL_COLOR_ATTACHMENT1, mFboNormal.handle(), 0);
		glNamedFramebufferTexture(mFbo.handle(), GL_DEPTH_ATTACHMENT, mFboDepth.handle(), 0);
		GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glNamedFramebufferDrawBuffers(mFbo.handle(), 2, bufs);

		mFboMixed = rvo::Framebuffer::CreateInfo{};
		mFboMixedColor = { {
			.levels = 1,
			.internalFormat = GL_RGBA16F,
			.width = mSize.x,
			.height = mSize.y,
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR,
			.wrap = GL_CLAMP_TO_EDGE,
			.anisotropy = 1.0f,
		} };

		glNamedFramebufferTexture(mFboMixed.handle(), GL_COLOR_ATTACHMENT0, mFboMixedColor.handle(), 0);

		mFboFinalColor = { {
				.levels = 1,
				.internalFormat = GL_RGBA8,
				.width = mSize.x,
				.height = mSize.y,
				.minFilter = GL_LINEAR,
				.magFilter = GL_LINEAR,
				.wrap = GL_CLAMP_TO_EDGE,
				.anisotropy = 1.0f,
			} };

		mFboFinal = rvo::Framebuffer::CreateInfo{};
		glNamedFramebufferTexture(mFboFinal.handle(), GL_COLOR_ATTACHMENT0, mFboFinalColor.handle(), 0);
	}

	
	void InstanceRendererData::init(std::size_t aInstancesPerDraw) {
		mInstancesPerDraw = aInstancesPerDraw;

		mBuffer = { {
				.data = std::span(static_cast<std::byte const*>(nullptr), aInstancesPerDraw * sizeof(glm::mat4)),
				.flags = GL_MAP_WRITE_BIT,
			} };
	}

	std::span<glm::mat4> InstanceRendererData::begin(GLsizeiptr numInstances) {
		return std::span(static_cast<glm::mat4*>(glMapNamedBufferRange(mBuffer.handle(), 0, numInstances * sizeof(glm::mat4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)), mInstancesPerDraw);
	}

	void InstanceRendererData::finish() {
		glUnmapNamedBuffer(mBuffer.handle());
	}

	void Renderer::init(rvo::AssetManager& aAssetManager) {
		mShaderProgramComposite = aAssetManager.get_shader_program("shaders/composite.glsl");
		mShaderProgramFinal = aAssetManager.get_shader_program("shaders/final.glsl");

		bloomRenderer.init(aAssetManager);

		mEngineShaderBuffer = { {
				.data = std::as_bytes(std::span(static_cast<EngineShaderData*>(nullptr), 1)),
				.flags = GL_DYNAMIC_STORAGE_BIT,
			} };

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, mEngineShaderBuffer.handle());

		mInstanceRendererData.init(1024);
	}

	void Renderer::render_scene(glm::ivec2 aTargetSize, entt::registry& aRegistry, Transform const& aTransform, Camera aCamera, float aCurrentTime) {
		mTargetSize = aTargetSize;

		if (aCamera.aspect == 0.0f) {
			aCamera.aspect = static_cast<float>(aTargetSize.x) / static_cast<float>(aTargetSize.y);
		}

		mGBuffers.resize(aTargetSize);

		mEngineShaderData.farPlane = aCamera.clippingPlanes[1];
		mEngineShaderData.time = aCurrentTime;
		mEngineShaderData.projection = glm::perspective(glm::radians(aCamera.fov), aCamera.aspect, aCamera.clippingPlanes[0], aCamera.clippingPlanes[1]);
		mEngineShaderData.view = glm::inverse(aTransform.get());
		glNamedBufferSubData(mEngineShaderBuffer.handle(), 0, sizeof(EngineShaderData), &mEngineShaderData);

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		mGBuffers.mFbo.bind();
		glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (mWireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		struct MeshMaterialPairHash {
			std::size_t operator()(const std::pair<std::shared_ptr<rvo::Mesh>, std::shared_ptr<rvo::Material>>& p) const {
				auto h1 = std::hash<std::shared_ptr<rvo::Mesh>>{}(p.first);
				auto h2 = std::hash<std::shared_ptr<rvo::Material>>{}(p.second);
				return h1 ^ (h2 << 1); // combine hashes
			}
		};

		static std::unordered_map<std::pair<std::shared_ptr<rvo::Mesh>, std::shared_ptr<rvo::Material>>, std::vector<rvo::Transform>, MeshMaterialPairHash> entitiesSorted;
		entitiesSorted.clear();

		// Find sun, and set sun direction to match its direction
		{
			// Default case is to point down if no sun object is found
			mEngineShaderData.sunDirection = glm::vec3(0.0f, -1.0f, 0.0f);

			for (auto [entity, gameObject, directionalLight] : aRegistry.view<rvo::GameObject, rvo::DirectionalLight>().each()) {
				if (!gameObject.mEnabled) continue;

				mEngineShaderData.sunDirection = gameObject.mTransform.forward();
				break;
			}
		}

		mNumBatches = 0;
		mNumEntities = 0;

		for (auto [entity, gameObject, meshRenderer] : aRegistry.view<rvo::GameObject, rvo::MeshRenderer>().each()) {
			if (!gameObject.mEnabled) continue;
			if (!meshRenderer.mMaterial) continue;
			if (!meshRenderer.mMaterial->mShaderProgram) continue;
			if (!meshRenderer.mMesh) continue;

			entitiesSorted[std::make_pair(meshRenderer.mMesh, meshRenderer.mMaterial)].push_back(gameObject.mTransform);
			++mNumEntities;
		}

		for (auto const& [key, items] : entitiesSorted) {
			auto const& [mesh, material] = key;

			if (!material->mShaderProgram->mBackfaceCull) {
				glDisable(GL_CULL_FACE);
			}

			mesh->bind();
			material->mShaderProgram->bind();
			if (material->mTexture) material->mTexture->bind(0);

			for (const auto& [key, field] : material->mFields) {
				if (auto const* value = std::any_cast<glm::vec3>(&field)) {
					material->mShaderProgram->push_3f(key, *value);
				}
			}

			std::size_t remainingItems = items.size();
			std::size_t idx = 0;

			while (remainingItems > 0) {
				std::size_t numElementsThisDraw = glm::min(remainingItems, mInstanceRendererData.mInstancesPerDraw);

				auto data = mInstanceRendererData.begin(numElementsThisDraw);

				for (std::size_t i = 0; i < numElementsThisDraw; ++i) {
					data[i] = items[idx + i].get();
				}

				mInstanceRendererData.finish();

				idx += numElementsThisDraw;
				remainingItems -= numElementsThisDraw;

				mesh->draw(static_cast<GLsizei>(numElementsThisDraw));
				++mNumBatches;

			}

			if (!material->mShaderProgram->mBackfaceCull) {
				glEnable(GL_CULL_FACE);
			}
		}

		if (mWireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		mGBuffers.mFboMixed.bind();
		mGBuffers.mFboAlbedo.bind(0);
		mGBuffers.mFboNormal.bind(1);
		mGBuffers.mFboDepth.bind(2);
		mShaderProgramComposite->bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);

		bloomRenderer.render(mGBuffers.mSize, mGBuffers.mFboMixedColor, mFilterRadius, aCamera.aspect);

		mGBuffers.mFboFinal.bind();
		glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

		mShaderProgramFinal->bind();
		mShaderProgramFinal->push_1f("uBlend", mBloomBlend);
		mGBuffers.mFboMixedColor.bind(0);
		bloomRenderer.mip_chain().bind(1);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	rvo::Texture& Renderer::get_output_texture() {
		return mGBuffers.mFboFinalColor;
	}
}