#inject

#ifdef RVO_VERT

#include "engine_data.glsl"

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoord;

layout(location = 12) in mat4 uTransform;

void main(void) {
    gl_Position = gProjection * gView * uTransform * vec4(iPosition, 1.0);
}

#endif

#ifdef RVO_FRAG

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;

uniform vec3 uColor;

void main(void) {
    oColor = vec4(uColor, 1.0);
    oNormal = vec4(0.0);
}

#endif