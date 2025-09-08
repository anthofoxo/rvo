#include <glad/gl.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <spdlog/spdlog.h>

#include "rvo_window.hpp"
#include "rvo_utility.hpp"
#include "rvo_gfx.hpp"
#include "rvo_renderer_bloom.hpp"
#include "rvo_rdoc.hpp"
#include "rvo_asset_manager.hpp"

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

struct Transform final {
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	glm::quat orientation = glm::identity<glm::quat>();
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
	
	glm::mat4 get() const {
		constexpr glm::vec3 skew = { 0.0f, 0.0f, 0.0f };
		constexpr glm::vec4 perspective = { 0.0f, 0.0f, 0.0f, 1.0f };
		return glm::recompose(scale, orientation, position, skew, perspective);
	}

	void set(glm::mat4 const& matrix) {
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(matrix, scale, orientation, position, skew, perspective);
	}

	void translate(glm::vec3 const& aValue) {
		glm::mat4 const result = glm::translate(get(), aValue);
		position = result[3];
	}
};

struct GameObject final {
	bool mEnabled = true;
	Transform mTransform;
	std::string mName = "Unnamed";
};

struct MeshRenderer final {
	std::shared_ptr<rvo::ShaderProgram> mShaderProgram;
	std::shared_ptr<rvo::Mesh> mMesh;
	std::shared_ptr<rvo::Texture> mTexture;
	std::unordered_map<std::string, std::any> mFields;
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

		if (!GLAD_GL_ARB_shading_language_include) {
			spdlog::critical("GL_ARB_shading_language_include is required to continue");
			throw std::runtime_error("Cannot continue, GL_ARB_shading_language_include is required");
		}

		for (auto const& entry : std::filesystem::directory_iterator("shaders/include")) {
			if (!entry.is_regular_file()) continue;

			std::string stem = std::string("/") + entry.path().filename().generic_string();

			auto data = rvo::read_file_bytes(entry.path()).value();
			
			glNamedStringARB(GL_SHADER_INCLUDE_ARB, stem.size(), stem.data(), data.size(), reinterpret_cast<GLchar const*>(data.data()));
		}

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

		glfwSwapInterval(1); // ensure `mVsync == true`

		imgui_init(mWindow.handle());

		spdlog::info("GL_RENDERER: {}", reinterpret_cast<char const*>(glGetString(GL_RENDERER)));
		spdlog::info("GL_VENDOR: {}", reinterpret_cast<char const*>(glGetString(GL_VENDOR)));
		spdlog::info("GL_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_VERSION)));
		spdlog::info("GL_SHADING_LANGUAGE_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

		mShaderProgramComposite = mAssetManager.get_shader_program("shaders/composite.glsl");

		bloomRenderer.init(mAssetManager);

		mCameraTransform.position = { 0.0f, 1.5f, 5.0f };

		for (int x = 0; x < 10; ++x) {
			for (int z = 0; z < 10; ++z) {
				entt::handle entity = { mRegistry, mRegistry.create() };
				auto& gameObject = entity.emplace<GameObject>();
				auto& meshRenderer = entity.emplace<MeshRenderer>();

				gameObject.mTransform.position.x = x * 3 - 15;
				gameObject.mTransform.position.z = z * 3 - 15;

				gameObject.mName = std::format("Yeen {}_{}", x, z);
				meshRenderer.mShaderProgram = mAssetManager.get_shader_program("shaders/opaque.glsl");
				meshRenderer.mMesh = mAssetManager.get_mesh("meshes/yeen.ply");
				meshRenderer.mTexture = mAssetManager.get_texture("textures/yeen_a.png");
			}
		}

		{
			entt::handle entity = { mRegistry, mRegistry.create() };
			auto& gameObject = entity.emplace<GameObject>();
			auto& meshRenderer = entity.emplace<MeshRenderer>();

			gameObject.mName = "Terrain";
			meshRenderer.mShaderProgram = mAssetManager.get_shader_program("shaders/terrain.glsl");
			meshRenderer.mMesh = mAssetManager.get_mesh("meshes/terrain.ply");
			meshRenderer.mTexture = mAssetManager.get_texture("textures/grass.png");
		}

		// Lights
		{
			entt::handle entity = { mRegistry, mRegistry.create() };
			auto& gameObject = entity.emplace<GameObject>();
			auto& meshRenderer = entity.emplace<MeshRenderer>();

			gameObject.mName = "Blue Light";
			gameObject.mTransform.position = { -8.0f, 5.0f, -20.0f };

			meshRenderer.mShaderProgram = mAssetManager.get_shader_program("shaders/color.glsl");
			meshRenderer.mMesh = mAssetManager.get_mesh("meshes/cube.ply");
			meshRenderer.mFields["uColor"] = glm::vec3(0.0f, 0.05f, 1.0f) * 100.0f;
		}

		{
			entt::handle entity = { mRegistry, mRegistry.create() };
			auto& gameObject = entity.emplace<GameObject>();
			auto& meshRenderer = entity.emplace<MeshRenderer>();

			gameObject.mName = "Red Light";
			gameObject.mTransform.position = { -10.0f, 2.0f, 10.0f };

			meshRenderer.mShaderProgram = mAssetManager.get_shader_program("shaders/color.glsl");
			meshRenderer.mMesh = mAssetManager.get_mesh("meshes/cube.ply");
			meshRenderer.mFields["uColor"] = glm::vec3(1.0f, 0.0f, 0.0f) * 100.0f;
		}

		{
			entt::handle entity = { mRegistry, mRegistry.create() };
			auto& gameObject = entity.emplace<GameObject>();
			auto& meshRenderer = entity.emplace<MeshRenderer>();

			gameObject.mName = "Green Light";
			gameObject.mTransform.position = { 30.0f, 8.0f, 2.0f };

			meshRenderer.mShaderProgram = mAssetManager.get_shader_program("shaders/color.glsl");
			meshRenderer.mMesh = mAssetManager.get_mesh("meshes/cube.ply");
			meshRenderer.mFields["uColor"] = glm::vec3(0.0f, 1.0f, 0.0f) * 100.0f;
		}
	}

	void update() {
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		// Is the viewport a different size?
		if (mFramebufferSize != mFboSize) {
			spdlog::debug("Viewport size changed from {}x{} to {}x{}. Recreating framebuffer", mFboSize.x, mFboSize.y, mFramebufferSize.x, mFramebufferSize.y);
			mFboSize = mFramebufferSize;

			mFbo = rvo::Framebuffer::CreateInfo{};

			mFboColor = { {
				.levels = 1,
				.internalFormat = GL_R11F_G11F_B10F,
				.width = mFboSize.x,
				.height = mFboSize.y,
				.minFilter = GL_LINEAR,
				.magFilter = GL_LINEAR,
				.wrap = GL_CLAMP_TO_EDGE,
				.anisotropy = 1.0f,
			} };

			mFboDepth = { {
					.internalFormat = GL_DEPTH_COMPONENT16,
					.width = mFboSize.x,
					.height = mFboSize.y,
				} };

			glNamedFramebufferTexture(mFbo.handle(), GL_COLOR_ATTACHMENT0, mFboColor.handle(), 0);
			glNamedFramebufferRenderbuffer(mFbo.handle(), GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mFboDepth.handle());
		}

		mFbo.bind();
		glViewport(0, 0, mFboSize.x, mFboSize.y);

		glClearColor(glm::pow(0.7f, 2.2f), glm::pow(0.8f, 2.2f), glm::pow(0.9f, 2.2f), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		if (mCursorLocked) {
			glm::vec3 localUpWorld = glm::inverse(mCameraTransform.get()) * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);

			mCameraTransform.orientation = glm::rotate(mCameraTransform.orientation, glm::radians(mCursorDelta.x * 0.3f), localUpWorld);
			mCameraTransform.orientation = glm::rotate(mCameraTransform.orientation, glm::radians(mCursorDelta.y * 0.3f), { -1.0f, 0.0f, 0.0f });

			glm::vec3 direction{};
			if (mKeysDown.contains(GLFW_KEY_W)) --direction.z;
			if (mKeysDown.contains(GLFW_KEY_S)) ++direction.z;
			if (mKeysDown.contains(GLFW_KEY_A)) --direction.x;
			if (mKeysDown.contains(GLFW_KEY_D)) ++direction.x;
			if (mKeysDown.contains(GLFW_KEY_LEFT_SHIFT)) --direction.y;
			if (mKeysDown.contains(GLFW_KEY_SPACE)) ++direction.y;
			if (mKeysDown.contains(GLFW_KEY_LEFT_CONTROL)) direction *= 3.0f;

			mCameraTransform.translate(direction * 10.0f * static_cast<float>(mDeltaTime));
		}

		float const aspectRatio = static_cast<float>(mFramebufferSize.x) / static_cast<float>(mFramebufferSize.y);
		glm::mat4 const projection = glm::perspective(glm::radians(mFieldOfView), aspectRatio, mNearPlane, mFarPlane);
		glm::mat4 const view = glm::inverse(mCameraTransform.get());

		for (auto [entity, gameObject, meshRenderer] : mRegistry.view<GameObject, MeshRenderer>().each()) {
			if (!gameObject.mEnabled) continue;

			meshRenderer.mShaderProgram->bind();
			meshRenderer.mShaderProgram->push_mat4f("uProjection", projection);
			meshRenderer.mShaderProgram->push_mat4f("uView", view);
			meshRenderer.mShaderProgram->push_mat4f("uTransform", gameObject.mTransform.get());

			// Fields
			for(const auto& [key, field] : meshRenderer.mFields) {
				if (auto const* value = std::any_cast<glm::vec3>(&field)) {
					meshRenderer.mShaderProgram->push_3f(key, *value);
				}
				else {
					spdlog::warn("Unsupported material field type `{}` on entity `{}`", field.type().name(), gameObject.mName);
					gameObject.mEnabled = false;
				}
			}

			if (meshRenderer.mTexture) meshRenderer.mTexture->bind(0);
			meshRenderer.mMesh->render();
		}

		bloomRenderer.render(mFboSize, mFboColor, mFilterRadius, aspectRatio);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, mFboSize.x, mFboSize.y);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		mShaderProgramComposite->bind();
		mShaderProgramComposite->push_1f("uBlend", mBloomBlend);
		mFboColor.bind(0);
		bloomRenderer.mip_chain().bind(1);
		glDrawArrays(GL_TRIANGLES, 0, 3);
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

				ImGui::EndMainMenuBar();
			}

			if (imguiDemoWindow) ImGui::ShowDemoWindow(&imguiDemoWindow);

			if (ImGui::Begin("Debug")) {
				ImGui::SliderFloat("Bloom Blend", &mBloomBlend, 0.0f, 1.0f);
				ImGui::SliderFloat("Bloom Upscale Filter Radius (ST)", &mFilterRadius, 0.0001f, 0.1f);
				ImGui::SliderFloat("Field of View", &mFieldOfView, 20.0f, 160.0f);
				ImGui::SliderFloat("Near Plane", &mNearPlane, 0.001f, 10.0f);
				ImGui::SliderFloat("Far Plane", &mFarPlane, 10.0f, 1024.0f);
				if (ImGui::Checkbox("VSync", &mVsync)) {
					glfwSwapInterval(mVsync ? 1 : 0);
				}
				
				ImGui::LabelText("Cursor Pos", "%.1f x %.1f", mCursorPos.x, mCursorPos.y);
				ImGui::LabelText("Cursor Delta", "%.1f x %.1f", mCursorDelta.x, mCursorDelta.y);

				ImGui::LabelText("Frametime", "%f", mDeltaTime);
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

			static entt::entity selected = entt::null;

			if (ImGui::Begin("Entities")) {
				for (auto [entity, gameObject] : mRegistry.view<GameObject>().each()) {
					ImGui::PushID(static_cast<int>(entity));

					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
					if (entity == selected) flags |= ImGuiTreeNodeFlags_Selected;
					bool opened = ImGui::TreeNodeEx(gameObject.mName.c_str(), flags);

					if (ImGui::IsItemActivated()) {
						selected = entity;
					}

					if (opened) {
						ImGui::TreePop();
					}

					ImGui::PopID();
				}
			}
			ImGui::End();

			if (ImGui::Begin("Properties")) {
				if (selected != entt::null) {
					GameObject& gameObject = mRegistry.get<GameObject>(selected);

					if (ImGui::CollapsingHeader("GameObject")) {
						ImGui::Checkbox("Enabled", &gameObject.mEnabled);
						ImGui::InputText("Name", &gameObject.mName);
						ImGui::Separator();

						glm::vec3 const euler = glm::degrees(glm::eulerAngles(gameObject.mTransform.orientation));
						glm::vec3 newEuler = euler;

						ImGui::DragFloat3("Position", glm::value_ptr(gameObject.mTransform.position), 0.1f);
						if (ImGui::DragFloat3("Rotation", glm::value_ptr(newEuler))) {
							glm::vec3 const deltaEuler = glm::radians(euler - newEuler);
							gameObject.mTransform.orientation = glm::rotate(gameObject.mTransform.orientation, deltaEuler.x, { 1.0f, 0.0f, 0.0f });
							gameObject.mTransform.orientation = glm::rotate(gameObject.mTransform.orientation, deltaEuler.y, { 0.0f, 1.0f, 0.0f });
							gameObject.mTransform.orientation = glm::rotate(gameObject.mTransform.orientation, deltaEuler.z, { 0.0f, 0.0f, 1.0f });
						}
						ImGui::DragFloat3("Scale", glm::value_ptr(gameObject.mTransform.scale), 0.1f);

						if (ImGui::SmallButton("Reset Transform")) {
							gameObject.mTransform = Transform{};
						}

						ImGui::SameLine();

						if (ImGui::SmallButton("Reset Orientation")) {
							gameObject.mTransform.orientation = glm::identity<glm::quat>();
						}
					}

					if (auto* component = mRegistry.try_get<MeshRenderer>(selected)) {
						if (ImGui::CollapsingHeader("MeshRenderer")) {
							ImGui::SeparatorText("Fields");

							for (auto& [key, field] : component->mFields) {
								if (auto* value = std::any_cast<glm::vec3>(&field)) {
									ImGui::DragFloat3(key.c_str(), glm::value_ptr(*value));
								}
							}
						}
					}
				}
			}
			ImGui::End();

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

	rvo::AssetManager mAssetManager;
	entt::registry mRegistry;

	std::unordered_set<int> mKeysDown;
	Transform mCameraTransform;

	glm::vec2 mCursorPosLast{};
	glm::vec2 mCursorPos{};
	glm::vec2 mCursorDelta{};

	glm::ivec2 mFramebufferSize;
	glm::ivec2 mFboSize{};
	rvo::Texture mFboColor;
	rvo::Renderbuffer mFboDepth;
	rvo::Framebuffer mFbo;

	float mBloomBlend = 0.02f;
	float mFilterRadius = 0.003f;
	float mNearPlane = 0.1f;
	float mFarPlane = 256.0f;
	float mFieldOfView = 90.0f;
	bool mCursorLocked = false;
	bool mVsync = true;

	rvo::BloomRenderer bloomRenderer;

	std::shared_ptr<rvo::ShaderProgram> mShaderProgramComposite;
};

namespace rvo {
	void entrypoint() {
		Application app;
		app.run();
	}
}