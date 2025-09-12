#include <glad/gl.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include <lua.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include "rvo_components.hpp"
#include "rvo_gui_panels.hpp"
#include "rvo_window.hpp"
#include "rvo_utility.hpp"
#include "rvo_gfx.hpp"
#include "rvo_renderer_bloom.hpp"
#include "rvo_rdoc.hpp"
#include "rvo_asset_manager.hpp"
#include "rvo_transform.hpp"

#include <entt/entt.hpp>

#include <stdexcept>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <any>


void imgui_init(GLFWwindow *aWindow) {
#ifdef _DEBUG
	IMGUI_CHECKVERSION();
#endif
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontFromFileTTF("NotoSans-Regular.ttf", 18.0f);

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()));

	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplGlfw_InitForOpenGL(aWindow, true);
	ImGui_ImplOpenGL3_Init("#version 460 core");
}

void configure_khr_debug() {
	auto message_callback = [](GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* user_param) -> void {
			spdlog::info("OpenGL Message: {} ({})", msg, id);

#ifdef _WIN32
			if (severity == GL_DEBUG_SEVERITY_HIGH) {
				__debugbreak();
			}
#endif
		};
	
	glEnable(GL_DEBUG_OUTPUT);
#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
	glDebugMessageCallback(message_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

struct EngineShaderData final {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
	alignas(16) glm::vec3 sunDirection;
	alignas(4)  float farPlane;
	alignas(4)  float time;
};

glm::quat quatLookAt(glm::vec3 direction, glm::vec3 up = glm::vec3(0, 1, 0)) {
	direction = glm::normalize(direction);

	// Handle degenerate up (parallel to direction)
	if (glm::abs(glm::dot(direction, up)) > 0.999f) {
		up = glm::vec3(1, 0, 0); // pick a fallback up
	}

	// Build right/up using -Z forward convention
	glm::vec3 forward = -direction; // because -Z is forward
	glm::vec3 right = glm::normalize(glm::cross(up, forward));
	glm::vec3 newUp = glm::cross(forward, right);

	// Build rotation matrix (column-major)
	glm::mat3 rotationMatrix(right, newUp, forward);

	return glm::quat_cast(rotationMatrix);
}

void load_scene(entt::registry& aRegistry, rvo::AssetManager& aAssetManager) {
	// auto create sun
	{
		entt::handle entity = { aRegistry, aRegistry.create() };
		auto& gameObject = entity.emplace<rvo::GameObject>();
		gameObject.mName = "Sun";
		gameObject.mTransform.position = { 0.0f, 10.0f, 0.0f };
		gameObject.mTransform.orientation = quatLookAt(glm::vec3(0.0f, -1.0f, 0.0f));

		auto& directionalLight = entity.emplace<rvo::DirectionalLight>();
		directionalLight.color = glm::vec3(1.0f);
	}

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	if (luaL_dofile(L, "scene.lua") != LUA_OK) {
		spdlog::error("Error while parsing scene: {}", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
	else {
		// Scene table at idx -1

		lua_pushnil(L);
		while (lua_next(L, -2)) {
			entt::handle entity = { aRegistry, aRegistry.create() };

			/* uses 'key' (at index -2) and 'value' (at index -1) */

			if (lua_getfield(L, -1, "GameObject") == LUA_TTABLE) {
				auto& component = entity.emplace<rvo::GameObject>();

				if (lua_getfield(L, -1, "name") == LUA_TSTRING) {
					component.mName = lua_tostring(L, -1);
				}
				lua_pop(L, 1);

				if (lua_getfield(L, -1, "transform") == LUA_TTABLE) {
					if (lua_getfield(L, -1, "position") == LUA_TTABLE && lua_rawlen(L, -1) == 3) {
						for (int i = 0; i < 3; ++i) {
							lua_rawgeti(L, -1, i + 1);
							component.mTransform.position[i] = static_cast<float>(lua_tonumber(L, -1));
							lua_pop(L, 1);
						}
					}
					lua_pop(L, 1);

					if (lua_getfield(L, -1, "scale") == LUA_TTABLE && lua_rawlen(L, -1) == 3) {
						for (int i = 0; i < 3; ++i) {
							lua_rawgeti(L, -1, i + 1);
							component.mTransform.scale[i] = static_cast<float>(lua_tonumber(L, -1));
							lua_pop(L, 1);
						}
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);

			if (lua_getfield(L, -1, "MeshRenderer") == LUA_TTABLE) {
				auto& component = entity.emplace<rvo::MeshRenderer>();

				if (lua_getfield(L, -1, "material") == LUA_TSTRING) {
					component.mMaterial = aAssetManager.get_material(lua_tostring(L, -1));
				}
				lua_pop(L, 1);

				if (lua_getfield(L, -1, "mesh") == LUA_TSTRING) {
					component.mMesh = aAssetManager.get_mesh(lua_tostring(L, -1));
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);

			lua_pop(L, 1);
		}

		lua_pop(L, 1); // Pop scene table
	}

	lua_close(L);
}

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

	void resize(glm::ivec2 const& aTargetSize) {
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
};

struct InstanceRendererData {
	rvo::Buffer mBuffer;
	std::size_t mInstancesPerDraw;

	void init(std::size_t aInstancesPerDraw) {
		mInstancesPerDraw = aInstancesPerDraw;

		mBuffer = { {
				.data = std::span(static_cast<std::byte const*>(nullptr), aInstancesPerDraw * sizeof(glm::mat4)),
				.flags = GL_MAP_WRITE_BIT,
			} };
	}

	std::span<glm::mat4> begin(GLsizeiptr numInstances) {
		return std::span(static_cast<glm::mat4*>(glMapNamedBufferRange(mBuffer.handle(), 0, numInstances * sizeof(glm::mat4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)), mInstancesPerDraw);
	}

	void finish() {
		glUnmapNamedBuffer(mBuffer.handle());
	}
};

struct Application final {
	void set_cursor_lock(bool aLocked) {
		mCursorLocked = aLocked;

		if (mCursorLocked) {
			glfwSetInputMode(mWindow.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
		}
		else {
			glfwSetInputMode(mWindow.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
		}
	}

	void init() {
		mWindow = rvo::Window{ {
			.width = 1280,
			.height = 720,
			.title = "Game",
			} };

		glfwSetWindowUserPointer(mWindow.handle(), this);

		// configure input listeners
		glfwSetKeyCallback(mWindow.handle(), [](GLFWwindow* window, int key, int scancode, int action, int mods) {
				Application& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

				switch (action) {
				case GLFW_PRESS:
					app.mKeysDown.emplace(key);
					break;
				case GLFW_RELEASE:
					app.mKeysDown.erase(key);
					break;
				}

				if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
					app.set_cursor_lock(!app.mCursorLocked);
				}

			});

		glfwSetWindowFocusCallback(mWindow.handle(), [](GLFWwindow* window, int focused) {
				if (focused == GLFW_FALSE) {
					Application& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
					app.set_cursor_lock(false);
				}
			});

		glfwSetCursorPosCallback(mWindow.handle(), [](GLFWwindow* window, double xpos, double ypos) {
				Application& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
				app.mCursorPos = glm::dvec2(xpos, ypos);
			});

		configure_khr_debug();

		glfwSwapInterval(mSwapInterval);

		imgui_init(mWindow.handle());

		spdlog::info("GL_RENDERER: {}", reinterpret_cast<char const*>(glGetString(GL_RENDERER)));
		spdlog::info("GL_VENDOR: {}", reinterpret_cast<char const*>(glGetString(GL_VENDOR)));
		spdlog::info("GL_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_VERSION)));
		spdlog::info("GL_SHADING_LANGUAGE_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

		mShaderProgramFinal = mAssetManager.get_shader_program("shaders/final.glsl");
		mShaderProgramComposite = mAssetManager.get_shader_program("shaders/composite.glsl");

		bloomRenderer.init(mAssetManager);

		mCameraTransform.position = { 0.0f, 1.5f, 5.0f };

		mEngineShaderBuffer = { {
				.data = std::as_bytes(std::span(static_cast<EngineShaderData*>(nullptr), 1)),
				.flags = GL_DYNAMIC_STORAGE_BIT,
			} };

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, mEngineShaderBuffer.handle());

		mInstanceRendererData.init(1024);

		load_scene(mRegistry, mAssetManager);
	}

	void update() {
		if (ImGui::Begin("Viewport")) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 cursor = ImGui::GetCursorPos();

			if (avail.x > 0 && avail.y > 0) {

				mAssetManager.update();

				mGBuffers.resize(glm::vec2(avail.x, avail.y));

				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glCullFace(GL_BACK);
				glFrontFace(GL_CCW);

				if (mCursorLocked) {
					glm::vec3 localUpWorld = glm::inverse(mCameraTransform.get()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

					float sensitivity = -mSensitivity;
					if (mKeysDown.contains(GLFW_KEY_C)) sensitivity /= 4.0f;

					mCameraTransform.orientation = glm::rotate(mCameraTransform.orientation, glm::radians(mCursorDelta.x * sensitivity), localUpWorld);
					mCameraTransform.orientation = glm::rotate(mCameraTransform.orientation, glm::radians(mCursorDelta.y * sensitivity), { 1.0f, 0.0f, 0.0f });

					glm::vec3 direction{};
					if (mKeysDown.contains(GLFW_KEY_W)) --direction.z;
					if (mKeysDown.contains(GLFW_KEY_S)) ++direction.z;
					if (mKeysDown.contains(GLFW_KEY_A)) --direction.x;
					if (mKeysDown.contains(GLFW_KEY_D)) ++direction.x;
					if (mKeysDown.contains(GLFW_KEY_LEFT_SHIFT)) --direction.y;
					if (mKeysDown.contains(GLFW_KEY_SPACE)) ++direction.y;
					if (mKeysDown.contains(GLFW_KEY_LEFT_CONTROL)) direction *= 3.0f;
					if (mKeysDown.contains(GLFW_KEY_LEFT_ALT)) direction /= 5.0f;

					mCameraTransform.translate(direction * 10.0f * static_cast<float>(mDeltaTime));
				}

				float const aspectRatio = static_cast<float>(mGBuffers.mSize.x) / static_cast<float>(mGBuffers.mSize.y);
				float const targetFov = mKeysDown.contains(GLFW_KEY_C) ? mFieldOfView / 4.0f : mFieldOfView;

				mEngineShaderData.farPlane = mClippingPlanes[1];
				mEngineShaderData.time = static_cast<float>(mCurrentTime);
				mEngineShaderData.projection = glm::perspective(glm::radians(targetFov), aspectRatio, mClippingPlanes[0], mClippingPlanes[1]);
				mEngineShaderData.view = glm::inverse(mCameraTransform.get());
				glNamedBufferSubData(mEngineShaderBuffer.handle(), 0, sizeof(EngineShaderData), &mEngineShaderData);

				mGBuffers.mFbo.bind();
				glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

				glClearColor(0, 0, 0, 0);
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

					for (auto [entity, gameObject, directionalLight] : mRegistry.view<rvo::GameObject, rvo::DirectionalLight>().each()) {
						if (!gameObject.mEnabled) continue;
						
						mEngineShaderData.sunDirection = gameObject.mTransform.forward();
						break;
					}
				}

				mDebugInfo.numBatches = 0;
				mDebugInfo.numEntities = 0;

				for (auto [entity, gameObject, meshRenderer] : mRegistry.view<rvo::GameObject, rvo::MeshRenderer>().each()) {
					if (!gameObject.mEnabled) continue;
					if (!meshRenderer.mMaterial) continue;
					if (!meshRenderer.mMaterial->mShaderProgram) continue;
					if (!meshRenderer.mMesh) continue;

					entitiesSorted[std::make_pair(meshRenderer.mMesh, meshRenderer.mMaterial)].push_back(gameObject.mTransform);
					++mDebugInfo.numEntities;
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

						mesh->draw(numElementsThisDraw);
						++mDebugInfo.numBatches;

					}
						
					if (!material->mShaderProgram->mBackfaceCull) {
						glEnable(GL_CULL_FACE);
					}
				}

				if (mWireframe) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}

				// Data is now inside gbuffers, perform composite pass

				mGBuffers.mFboMixed.bind();
				mGBuffers.mFboAlbedo.bind(0);
				mGBuffers.mFboNormal.bind(1);
				mGBuffers.mFboDepth.bind(2);
				mShaderProgramComposite->bind();
				glDrawArrays(GL_TRIANGLES, 0, 3);

				bloomRenderer.render(mGBuffers.mSize, mGBuffers.mFboMixedColor, mFilterRadius, aspectRatio);

				mGBuffers.mFboFinal.bind();
				glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

				mShaderProgramFinal->bind();
				mShaderProgramFinal->push_1f("uBlend", mBloomBlend);
				mGBuffers.mFboMixedColor.bind(0);
				bloomRenderer.mip_chain().bind(1);
				glDrawArrays(GL_TRIANGLES, 0, 3);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				ImGui::Image(mGBuffers.mFboFinalColor.handle(), avail, { 0, 1 }, { 1, 0 });

				if (mEditorState.mEditorSelection != entt::null) {
					static auto operation = ImGuizmo::OPERATION::TRANSLATE;
					static auto mode = ImGuizmo::MODE::LOCAL;

					if (ImGui::IsWindowFocused()) {
						if (ImGui::IsKeyPressed(ImGuiKey_W)) { operation = ImGuizmo::OPERATION::TRANSLATE; }
						if (ImGui::IsKeyPressed(ImGuiKey_E)) { operation = ImGuizmo::OPERATION::ROTATE; }
						if (ImGui::IsKeyPressed(ImGuiKey_R)) { operation = ImGuizmo::OPERATION::SCALE; }
						
						if (ImGui::IsKeyPressed(ImGuiKey_X)) {
							if (mode == ImGuizmo::MODE::LOCAL) {
								mode = ImGuizmo::MODE::WORLD;
							}
							else {
								mode = ImGuizmo::MODE::LOCAL;
							}
							
						}
					}

					auto& transform = mRegistry.get<rvo::GameObject>(mEditorState.mEditorSelection).mTransform;
					glm::mat4 matrix = transform.get();
					ImGuizmo::SetDrawlist();
					ImGuizmo::SetRect(ImGui::GetWindowPos().x + cursor.x, ImGui::GetWindowPos().y + cursor.y, static_cast<float>(mGBuffers.mSize.x), static_cast<float>(mGBuffers.mSize.y));
					if (ImGuizmo::Manipulate(glm::value_ptr(mEngineShaderData.view), glm::value_ptr(mEngineShaderData.projection), operation, mode, glm::value_ptr(matrix))) {
						transform.set(matrix);
					}
				}

			}
		}
		ImGui::End();
	}

	void uninit() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void run() {
		init();

		double lastTime = glfwGetTime();
		bool imguiDemoWindow = false;

		while (mRunning) {
			mCurrentTime = glfwGetTime();
			mDeltaTime = mCurrentTime - lastTime;
			lastTime = mCurrentTime;

			mCursorPosLast = mCursorPos;
			glfwPollEvents();
			mCursorDelta = mCursorPos - mCursorPosLast;

			if (glfwWindowShouldClose(mWindow.handle())) mRunning = false;

			if (glfwGetWindowAttrib(mWindow.handle(), GLFW_ICONIFIED) != 0) {
				ImGui_ImplGlfw_Sleep(10);
				continue;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGuizmo::BeginFrame();

			ImGui::DockSpaceOverViewport();

			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("View")) {

					ImGui::MenuItem("Dear ImGui Demo", nullptr, &imguiDemoWindow);

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Debug")) {
					ImGui::SeparatorText("RenderDoc");

					if (ImGui::MenuItem("Trigger Capture", ImGui::GetKeyChordName(ImGuiKey_F8), nullptr, rvo::rdoc::attached())) {
						rvo::rdoc::trigger_capture();
					}

					if (ImGui::MenuItem("Launch Replay UI", nullptr, nullptr, rvo::rdoc::attached())) {
						rvo::rdoc::launch_replay_ui();
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Scene")) {

					if (ImGui::MenuItem("Reload")) {
						mRegistry.clear();
						load_scene(mRegistry, mAssetManager);
					}

					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (imguiDemoWindow) ImGui::ShowDemoWindow(&imguiDemoWindow);

			if (ImGui::Begin("Debug")) {
				ImGui::SliderFloat("Bloom Blend", &mBloomBlend, 0.0f, 1.0f);
				ImGui::SliderFloat("Bloom Upscale Filter Radius (ST)", &mFilterRadius, 0.0001f, 0.1f);
				ImGui::SliderFloat("Field of View", &mFieldOfView, 20.0f, 160.0f);

				ImGui::DragFloatRange2("Clipping Planes", &mClippingPlanes[0], &mClippingPlanes[1], 1.0f, 0.01f, 4096.0f);

				if (ImGui::SliderInt("Swap Interval", &mSwapInterval, 0, 10)) {
					glfwSwapInterval(mSwapInterval);
				}
				ImGui::Checkbox("Wireframe", &mWireframe);

				ImGui::SliderFloat("Sensitivity", &mSensitivity, 0.01f, 1.0f);
				

				ImGui::LabelText("Num Entities", "%d", mDebugInfo.numEntities);
				ImGui::LabelText("Num Batches", "%d", mDebugInfo.numBatches);

				ImGui::LabelText("Cursor Pos", "%.1f x %.1f", mCursorPos.x, mCursorPos.y);
				ImGui::LabelText("Cursor Delta", "%.1f x %.1f", mCursorDelta.x, mCursorDelta.y);

				ImGui::LabelText("Frametime", "%.5fms", mDeltaTime * 1000.0);
				ImGui::LabelText("Framerate", "%d", static_cast<int>(1.0 / mDeltaTime));
				ImGui::LabelText("Framebuffer", "%d x %d", mFramebufferSize.x, mFramebufferSize.y);

				ImGui::LabelText("GL_RENDERER", "%s", glGetString(GL_RENDERER));
				ImGui::LabelText("GL_VENDOR", "%s", glGetString(GL_VENDOR));
				ImGui::LabelText("GL_VERSION", "%s", glGetString(GL_VERSION));
				ImGui::LabelText("GL_SHADING_LANGUAGE_VERSION", "%s", glGetString(GL_SHADING_LANGUAGE_VERSION));
			}
			ImGui::End();

			glfwGetFramebufferSize(mWindow.handle(), &mFramebufferSize.x, &mFramebufferSize.y);
			
			update();

			rvo::gui_panel_entities(mRegistry, mEditorState);
			rvo::gui_panel_properties({ mRegistry, mEditorState.mEditorSelection });

			glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}

			mWindow.swap_buffers();
		}

		mRunning = false;

		uninit();
	}

	bool mRunning = true;
	rvo::Window mWindow;
	double mDeltaTime = 0.0;
	double mCurrentTime;

	struct DebugInfo final {
		int numEntities;
		int numBatches;

	} mDebugInfo;

	rvo::AssetManager mAssetManager;
	entt::registry mRegistry;

	std::unordered_set<int> mKeysDown;
	rvo::Transform mCameraTransform;

	glm::vec2 mCursorPosLast{};
	glm::vec2 mCursorPos{};
	glm::vec2 mCursorDelta{};

	glm::ivec2 mFramebufferSize;
	GBuffers mGBuffers;

	rvo::Buffer mEngineShaderBuffer;
	EngineShaderData mEngineShaderData;

	float mSensitivity = 0.3f;
	float mBloomBlend = 0.02f;
	float mFilterRadius = 0.003f;
	glm::vec2 mClippingPlanes = { 0.2f, 256.0f };
	float mFieldOfView = 90.0f;
	bool mCursorLocked = false;
	int mSwapInterval = 1;
	bool mWireframe = false;
	rvo::EditorState mEditorState;

	InstanceRendererData mInstanceRendererData;

	rvo::BloomRenderer bloomRenderer;

	std::shared_ptr<rvo::ShaderProgram> mShaderProgramFinal;
	std::shared_ptr<rvo::ShaderProgram> mShaderProgramComposite;
};

Application* gApp;

namespace rvo {
	GLuint get_instanced_buffer() {
		return gApp->mInstanceRendererData.mBuffer.handle();
	}
}

namespace rvo {
	void entrypoint() {
		Application app;
		gApp = &app;
		app.run();
	}
}