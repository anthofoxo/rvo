#include "rvo_framebuffer.hpp"

namespace rvo {
	Framebuffer::Framebuffer(CreateInfo const& aInfo) {
		glCreateFramebuffers(1, &mHandle);
	}

	Framebuffer& Framebuffer::operator=(Framebuffer&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Framebuffer::~Framebuffer() noexcept {
		if (mHandle) {
			glDeleteRenderbuffers(1, &mHandle);
		}
	}

	void Framebuffer::bind() const {
		glBindFramebuffer(GL_FRAMEBUFFER, mHandle);
	}
}