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

struct EngineShaderData final {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

void load_scene(entt::registry& aRegistry, rvo::AssetManager& aAssetManager) {
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
				auto& component = entity.emplace<GameObject>();

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
				auto& component = entity.emplace<MeshRenderer>();

				if (lua_getfield(L, -1, "shaderProgram") == LUA_TSTRING) {
					component.mShaderProgram = aAssetManager.get_shader_program(lua_tostring(L, -1));
				}
				lua_pop(L, 1);

				if (lua_getfield(L, -1, "mesh") == LUA_TSTRING) {
					component.mMesh = aAssetManager.get_mesh(lua_tostring(L, -1));
				}
				lua_pop(L, 1);

				if (lua_getfield(L, -1, "texture") == LUA_TSTRING) {
					component.mTexture = aAssetManager.get_texture(lua_tostring(L, -1));
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

							component.mFields[std::move(key)] = value;
						}
						else {
							spdlog::warn("Unsupported field: {}", key);
						}

						lua_pop(L, 2);
					}
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
	rvo::Texture mFboColor;
	rvo::Renderbuffer mFboDepth;
	rvo::Framebuffer mFbo;

	rvo::Texture mFboFinalColor;
	rvo::Framebuffer mFboFinal;

	void resize(glm::ivec2 const& aTargetSize) {
		if (aTargetSize == mSize) return;

		mSize = aTargetSize;

		mFbo = rvo::Framebuffer::CreateInfo{};

		mFboColor = { {
			.levels = 1,
			.internalFormat = GL_R11F_G11F_B10F,
			.width = mSize.x,
			.height = mSize.y,
			.minFilter = GL_LINEAR,
			.magFilter = GL_LINEAR,
			.wrap = GL_CLAMP_TO_EDGE,
			.anisotropy = 1.0f,
		} };

		mFboDepth = { {
				.internalFormat = GL_DEPTH_COMPONENT32F,
				.width = mSize.x,
				.height = mSize.y,
			} };

		glNamedFramebufferTexture(mFbo.handle(), GL_COLOR_ATTACHMENT0, mFboColor.handle(), 0);
		glNamedFramebufferRenderbuffer(mFbo.handle(), GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mFboDepth.handle());

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

		glfwSwapInterval(1); // ensure `mVsync == true`

		imgui_init(mWindow.handle());

		spdlog::info("GL_RENDERER: {}", reinterpret_cast<char const*>(glGetString(GL_RENDERER)));
		spdlog::info("GL_VENDOR: {}", reinterpret_cast<char const*>(glGetString(GL_VENDOR)));
		spdlog::info("GL_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_VERSION)));
		spdlog::info("GL_SHADING_LANGUAGE_VERSION: {}", reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

		mShaderProgramComposite = mAssetManager.get_shader_program("shaders/composite.glsl");

		bloomRenderer.init(mAssetManager);

		mCameraTransform.position = { 0.0f, 1.5f, 5.0f };

		mEngineShaderBuffer = { {
				.data = std::as_bytes(std::span(static_cast<EngineShaderData*>(nullptr), 1)),
				.flags = GL_DYNAMIC_STORAGE_BIT,
			} };

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, mEngineShaderBuffer.handle());

		load_scene(mRegistry, mAssetManager);
	}

	void update() {
		if (ImGui::Begin("Viewport")) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 cursor = ImGui::GetCursorPos();

			if (avail.x > 0 && avail.y > 0) {

				mGBuffers.resize(glm::vec2(avail.x, avail.y));

				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glCullFace(GL_BACK);
				glFrontFace(GL_CCW);

				if (mCursorLocked) {
					glm::vec3 localUpWorld = glm::inverse(mCameraTransform.get()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);


					float sensitivity = -0.3f;
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

				mEngineShaderData.projection = glm::perspective(glm::radians(targetFov), aspectRatio, mNearPlane, mFarPlane);
				mEngineShaderData.view = glm::inverse(mCameraTransform.get());
				glNamedBufferSubData(mEngineShaderBuffer.handle(), 0, sizeof(EngineShaderData), &mEngineShaderData);

				mGBuffers.mFbo.bind();
				glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

				glClearColor(glm::pow(0.7f, 2.2f), glm::pow(0.8f, 2.2f), glm::pow(0.9f, 2.2f), 1.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				if (mWireframe) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}

				for (auto [entity, gameObject, meshRenderer] : mRegistry.view<GameObject, MeshRenderer>().each()) {
					if (!gameObject.mEnabled) continue;

					if (!meshRenderer.mShaderProgram->mBackfaceCull) {
						glDisable(GL_CULL_FACE);
					}

					meshRenderer.mShaderProgram->bind();
					meshRenderer.mShaderProgram->push_mat4f("uTransform", gameObject.mTransform.get());

					for (const auto& [key, field] : meshRenderer.mFields) {
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

					if (!meshRenderer.mShaderProgram->mBackfaceCull) {
						glEnable(GL_CULL_FACE);
					}
				}

				if (mWireframe) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}

				bloomRenderer.render(mGBuffers.mSize, mGBuffers.mFboColor, mFilterRadius, aspectRatio);

				mGBuffers.mFboFinal.bind();
				glViewport(0, 0, mGBuffers.mSize.x, mGBuffers.mSize.y);

				mShaderProgramComposite->bind();
				mShaderProgramComposite->push_1f("uBlend", mBloomBlend);
				mGBuffers.mFboColor.bind(0);
				bloomRenderer.mip_chain().bind(1);
				glDrawArrays(GL_TRIANGLES, 0, 3);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				ImGui::Image(mGBuffers.mFboFinalColor.handle(), avail, { 0, 1 }, { 1, 0 });

				if (mEditorSelected != entt::null) {
					glm::mat4 matrix = mRegistry.get<GameObject>(mEditorSelected).mTransform.get();
					ImGuizmo::SetDrawlist();
					ImGuizmo::SetRect(ImGui::GetWindowPos().x + cursor.x, ImGui::GetWindowPos().y + cursor.y, static_cast<float>(mGBuffers.mSize.x), static_cast<float>(mGBuffers.mSize.y));
					if (ImGuizmo::Manipulate(glm::value_ptr(mEngineShaderData.view), glm::value_ptr(mEngineShaderData.projection), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(matrix))) {
						mRegistry.get<GameObject>(mEditorSelected).mTransform.set(matrix);
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
				ImGui::SliderFloat("Near Plane", &mNearPlane, 0.001f, 10.0f);
				ImGui::SliderFloat("Far Plane", &mFarPlane, 10.0f, 1024.0f);
				if (ImGui::Checkbox("VSync", &mVsync)) {
					glfwSwapInterval(mVsync ? 1 : 0);
				}
				ImGui::Checkbox("Wireframe", &mWireframe);
				
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

		
			if (ImGui::Begin("Entities")) {
				for (auto [entity, gameObject] : mRegistry.view<GameObject>().each()) {
					ImGui::PushID(static_cast<int>(entity));

					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
					if (entity == mEditorSelected) flags |= ImGuiTreeNodeFlags_Selected;
					bool opened = ImGui::TreeNodeEx(gameObject.mName.c_str(), flags);

					if (ImGui::IsItemActivated()) {
						mEditorSelected = entity;
					}

					if (opened) {
						ImGui::TreePop();
					}

					ImGui::PopID();
				}
			}
			ImGui::End();

			if (ImGui::Begin("Properties")) {
				if (mEditorSelected != entt::null) {
					GameObject& gameObject = mRegistry.get<GameObject>(mEditorSelected);

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

					if (auto* component = mRegistry.try_get<MeshRenderer>(mEditorSelected)) {
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
	Transform mCameraTransform;

	glm::vec2 mCursorPosLast{};
	glm::vec2 mCursorPos{};
	glm::vec2 mCursorDelta{};

	glm::ivec2 mFramebufferSize;
	GBuffers mGBuffers;

	rvo::Buffer mEngineShaderBuffer;
	EngineShaderData mEngineShaderData;

	float mBloomBlend = 0.02f;
	float mFilterRadius = 0.003f;
	float mNearPlane = 0.1f;
	float mFarPlane = 256.0f;
	float mFieldOfView = 90.0f;
	bool mCursorLocked = false;
	bool mVsync = true;
	bool mWireframe = false;
	entt::entity mEditorSelected = entt::null;

	rvo::BloomRenderer bloomRenderer;

	std::shared_ptr<rvo::ShaderProgram> mShaderProgramComposite;
};

namespace rvo {
	void entrypoint() {
		Application app;
		app.run();
	}
}