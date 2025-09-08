#include "rvo_buffer.hpp"

namespace rvo {
	Buffer::Buffer(CreateInfo const& aInfo) {
		glCreateBuffers(1, &mHandle);
		glNamedBufferStorage(mHandle, static_cast<GLsizeiptr>(aInfo.data.size_bytes()), aInfo.data.data(), aInfo.flags);
	}

	Buffer& Buffer::operator=(Buffer&& aOther) noexcept {
		std::swap(mHandle, aOther.mHandle);
		return *this;
	}

	Buffer::~Buffer() noexcept {
		if (mHandle) {
			glDeleteBuffers(1, &mHandle);
		}
	}
}