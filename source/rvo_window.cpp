#include "rvo_window.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>

#include <cstdint>

namespace rvo {
	namespace {
		std::int_fast8_t gWindowCount = 0;
	}

	Window::Window(CreateInfo const& aInfo) {
		if (gWindowCount == 0) {
			glfwSetErrorCallback([](int aErrorCode, const char* aDescription) {
					spdlog::critical("GLFW Error {}: {}", aErrorCode, aDescription);
				});

			if (!glfwInit()) {
				spdlog::critical("glfwInit failure");
				throw std::runtime_error("Cannot start, glfwInit failed");
			}
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

		mHandle = glfwCreateWindow(aInfo.width, aInfo.height, aInfo.title, nullptr, nullptr);

		if (!mHandle) {
			spdlog::critical("glfwCreateWindow failure");
			throw std::runtime_error("Cannot continue, glfwCreateWindow failed");
		}

		glfwMakeContextCurrent(mHandle);

		if (!gladLoadGL(glfwGetProcAddress)) {
			spdlog::critical("gladLoadGL failure");
			throw std::runtime_error("cannot continue, gladLoadGL failed");
		}

		++gWindowCount;
	}

	Window& Window::operator=(Window&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Window::~Window() noexcept {
		if (mHandle) {
			glfwMakeContextCurrent(nullptr);
			glfwDestroyWindow(mHandle);
			--gWindowCount;

			if (gWindowCount == 0) {
				glfwTerminate();
			}
		}
	}

	void Window::swap_buffers() const {
		glfwSwapBuffers(mHandle);
	}
}