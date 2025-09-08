#include "rvo_mesh.hpp"

#include "../happly.h"

#include "../rvo_utility.hpp"

#include <vector>
#include <cstdint>

namespace rvo {
	namespace {
		std::pair<std::vector<StandardVertex>, std::vector<std::uint32_t>> read_mesh(const char* aPath) {
			happly::PLYData plyIn(aPath);

			auto x = plyIn.getElement("vertex").getProperty<float>("x");
			auto y = plyIn.getElement("vertex").getProperty<float>("y");
			auto z = plyIn.getElement("vertex").getProperty<float>("z");
			auto nx = plyIn.getElement("vertex").getProperty<float>("nx");
			auto ny = plyIn.getElement("vertex").getProperty<float>("ny");
			auto nz = plyIn.getElement("vertex").getProperty<float>("nz");
			auto s = plyIn.getElement("vertex").getProperty<float>("s"); // UV are also possible options for this, blender exports ST so we use that
			auto t = plyIn.getElement("vertex").getProperty<float>("t");

			auto elements = plyIn.getElement("face").getListPropertyAnySign<std::size_t>("vertex_indices");

			std::vector<StandardVertex> vertices(x.size());
			for (size_t i = 0; i < x.size(); ++i) {
				vertices[i].position = glm::vec3(x[i], y[i], z[i]);
				vertices[i].normal = glm::vec3(nx[i], ny[i], nz[i]);

				// Flip uv on y axis so we dont need to flip images on load
				vertices[i].textureCoord = glm::vec2(s[i], 1.0f - t[i]);
			}

			std::vector<std::uint32_t> indices;
			// Preallocate the index buffer, assume all faces will have the same number of vertices
			// This guess is likely correct, but it's possible to be wrong, it's not a big deal if its wrong
			indices.reserve(elements.size() * (elements[0].size() - 2) * 3);

			for (auto& element : elements) {
				for (size_t j = 1; j + 1 < element.size(); j++) {
					indices.push_back(static_cast<std::uint32_t>(element[0]));
					indices.push_back(static_cast<std::uint32_t>(element[j]));
					indices.push_back(static_cast<std::uint32_t>(element[j + 1]));
				}
			}

			return { vertices, indices };
		}
	}

	Mesh::Mesh(char const* aPath) {
		auto [vertices, elements] = read_mesh(aPath);

		glCreateBuffers(1, &mVbo);
		glNamedBufferStorage(mVbo, rvo::size_bytes(vertices), vertices.data(), GL_NONE);
		glCreateBuffers(1, &mEbo);
		glNamedBufferStorage(mEbo, rvo::size_bytes(elements), elements.data(), GL_NONE);
		glCreateVertexArrays(1, &mVao);
		glVertexArrayVertexBuffer(mVao, 0, mVbo, 0, sizeof(StandardVertex));
		glVertexArrayElementBuffer(mVao, mEbo);
		glEnableVertexArrayAttrib(mVao, 0);
		glEnableVertexArrayAttrib(mVao, 1);
		glEnableVertexArrayAttrib(mVao, 2);
		glVertexArrayAttribFormat(mVao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(StandardVertex, position));
		glVertexArrayAttribFormat(mVao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(StandardVertex, normal));
		glVertexArrayAttribFormat(mVao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(StandardVertex, textureCoord));
		glVertexArrayAttribBinding(mVao, 0, 0);
		glVertexArrayAttribBinding(mVao, 1, 0);
		glVertexArrayAttribBinding(mVao, 2, 0);
		mCount = static_cast<GLsizei>(elements.size());
	}

	Mesh& Mesh::operator=(Mesh&& aOther) noexcept {
		std::swap(mVao, aOther.mVao);
		std::swap(mVbo, aOther.mVbo);
		std::swap(mEbo, aOther.mEbo);
		std::swap(mCount, aOther.mCount);
		return *this;
	}

	Mesh::~Mesh() noexcept {
		if (mVao) glDeleteVertexArrays(1, &mVao);
		if (mVbo) glDeleteBuffers(1, &mVbo);
		if (mEbo) glDeleteBuffers(1, &mEbo);
	}

	void Mesh::render() const {
		glBindVertexArray(mVao);
		glDrawElements(GL_TRIANGLES, mCount, GL_UNSIGNED_INT, nullptr);
	}
}