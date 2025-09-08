#inject

#ifdef RVO_VERT

#include "engine_data.glsl"

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoord;

out vec2 vTexCoord;
out vec3 vNormal;

uniform mat4 uTransform;

void main(void) {
    gl_Position = gProjection * gView * uTransform * vec4(iPosition, 1.0);
    vTexCoord = iTexCoord;
    vNormal = mat3(uTransform) * iNormal;
}

#endif

#ifdef RVO_FRAG

in vec2 vTexCoord;
in vec3 vNormal;

layout (location = 0) out vec4 oColor;

layout (binding = 0) uniform sampler2D tAlbedo;

void main(void) {
    oColor = texture(tAlbedo, vTexCoord);
    oColor.rgb *= max(0.2, dot(normalize(vNormal), vec3(0.0, 1.0, 0.0)));
}

#endif