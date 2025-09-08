#pragma once

#ifdef _WIN32
#	include <Windows.h> // Prevent APIENTRY Redefinition
#endif

#include <GLFW/glfw3.h>

#include <utility>

namespace rvo {
	class Window final {
	public:
		struct CreateInfo final {
			int width = 0;
			int height = 0;
			char const* title = nullptr;
		};

		constexpr Window() noexcept = default;
		Window(CreateInfo const& aInfo);
		Window(Window const&) = delete;
		Window& operator=(Window const&) = delete;
		Window(Window&& aOther) noexcept { *this = std::move(aOther); }
		Window& operator=(Window&& aOther) noexcept;
		~Window() noexcept;

		void swap_buffers() const;

		GLFWwindow* handle() const noexcept { return mHandle; }

	private:
		GLFWwindow* mHandle = nullptr;
	};
}