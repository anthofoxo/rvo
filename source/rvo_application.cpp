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
#include "rvo_rdoc.hpp"
#include "rvo_asset_manager.hpp"
#include "rvo_transform.hpp"
#include "rvo_renderer.hpp"

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

void load_scene(entt::registry& aRegistry, rvo::AssetManager& aAssetManager) {
	// auto create sun
	{
		entt::handle entity = { aRegistry, aRegistry.create() };
		auto& gameObject = entity.emplace<rvo::GameObject>();
		gameObject.mName = "Sun";
		gameObject.mTransform.position = { 0.0f, 10.0f, 0.0f };
		gameObject.mTransform.orientation = rvo::orient_in_direction(glm::vec3(0.0f, -1.0f, 0.0f));

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

		mRenderer.init(mAssetManager);

		mCameraTransform.position = { 0.0f, 1.5f, 5.0f };

		load_scene(mRegistry, mAssetManager);
	}

	void update() {
		if (ImGui::Begin("Viewport")) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 cursor = ImGui::GetCursorPos();

			if (avail.x > 0 && avail.y > 0) {
				mAssetManager.update();

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

				auto camera = mCamera; // Copy camera to allow fov modification
				if (mKeysDown.contains(GLFW_KEY_C)) camera.fov /= 4.0f;

				mRenderer.render_scene(std::bit_cast<glm::vec2>(avail), mRegistry, mCameraTransform, camera, static_cast<float>(mCurrentTime));

				ImGui::Image(mRenderer.get_output_texture().handle(), avail, { 0, 1 }, { 1, 0 });

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
					ImGuizmo::SetRect(ImGui::GetWindowPos().x + cursor.x, ImGui::GetWindowPos().y + cursor.y, static_cast<float>(mRenderer.mTargetSize.x), static_cast<float>(mRenderer.mTargetSize.y));
					if (ImGuizmo::Manipulate(glm::value_ptr(mRenderer.mEngineShaderData.view), glm::value_ptr(mRenderer.mEngineShaderData.projection), operation, mode, glm::value_ptr(matrix))) {
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
				ImGui::SliderFloat("Bloom Blend", &mRenderer.mBloomBlend, 0.0f, 1.0f);
				ImGui::SliderFloat("Bloom Upscale Filter Radius (ST)", &mRenderer.mFilterRadius, 0.0001f, 0.1f);
				ImGui::SliderFloat("Field of View", &mCamera.fov, 20.0f, 160.0f);

				ImGui::DragFloatRange2("Clipping Planes", &mCamera.clippingPlanes[0], &mCamera.clippingPlanes[1], 1.0f, 0.01f, 4096.0f);

				if (ImGui::SliderInt("Swap Interval", &mSwapInterval, 0, 10)) {
					glfwSwapInterval(mSwapInterval);
				}
				ImGui::Checkbox("Wireframe", &mRenderer.mWireframe);

				ImGui::SliderFloat("Sensitivity", &mSensitivity, 0.01f, 1.0f);

				ImGui::LabelText("Num Entities", "%d", mRenderer.mNumEntities);
				ImGui::LabelText("Num Batches", "%d", mRenderer.mNumBatches);

				ImGui::LabelText("Cursor Pos", "%.1f x %.1f", mCursorPos.x, mCursorPos.y);
				ImGui::LabelText("Cursor Delta", "%.1f x %.1f", mCursorDelta.x, mCursorDelta.y);

				ImGui::LabelText("Frametime", "%.5fms", mDeltaTime * 1000.0);
				ImGui::LabelText("Framerate", "%d", static_cast<int>(1.0 / mDeltaTime));
				ImGui::LabelText("Framebuffer", "%d x %d", mRenderer.mTargetSize.x, mRenderer.mTargetSize.y);

				ImGui::LabelText("GL_RENDERER", "%s", glGetString(GL_RENDERER));
				ImGui::LabelText("GL_VENDOR", "%s", glGetString(GL_VENDOR));
				ImGui::LabelText("GL_VERSION", "%s", glGetString(GL_VERSION));
				ImGui::LabelText("GL_SHADING_LANGUAGE_VERSION", "%s", glGetString(GL_SHADING_LANGUAGE_VERSION));
			}
			ImGui::End();

			glm::ivec2 viewportSize;
			glfwGetFramebufferSize(mWindow.handle(), &viewportSize.x, &viewportSize.y);
			
			update();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, viewportSize.x, viewportSize.y);

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

	rvo::AssetManager mAssetManager;
	entt::registry mRegistry;

	std::unordered_set<int> mKeysDown;
	rvo::Transform mCameraTransform;

	glm::vec2 mCursorPosLast{};
	glm::vec2 mCursorPos{};
	glm::vec2 mCursorDelta{};

	rvo::Camera mCamera;

	float mSensitivity = 0.3f;
	bool mCursorLocked = false;
	int mSwapInterval = 1;
	rvo::EditorState mEditorState;

	rvo::Renderer mRenderer;
};

Application* gApp;

namespace rvo {
	GLuint get_instanced_buffer() {
		return gApp->mRenderer.mInstanceRendererData.mBuffer.handle();
	}
}

namespace rvo {
	void entrypoint() {
		Application app;
		gApp = &app;
		app.run();
	}
}