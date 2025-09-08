#include "rvo_renderbuffer.hpp"

namespace rvo {
	Renderbuffer::Renderbuffer(CreateInfo const& aInfo) {
		glCreateRenderbuffers(1, &mHandle);
		glNamedRenderbufferStorage(mHandle, aInfo.internalFormat, aInfo.width, aInfo.height);
	}

	Renderbuffer& Renderbuffer::operator=(Renderbuffer&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Renderbuffer::~Renderbuffer() noexcept {
		if (mHandle) {
			glDeleteRenderbuffers(1, &mHandle);
		}
	}

}