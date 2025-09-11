#inject

#pragma RVO_NO_BACKFACE_CULL

#ifdef RVO_VERT

#include "engine_data.glsl"

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoord;

out vec2 vTexCoord;
out vec3 vNormal;

layout(location = 12) in mat4 uTransform;

void main(void) {
    vec4 worldSpace = uTransform * vec4(iPosition, 1.0);
    worldSpace.x += sin(gTime + worldSpace.x) * 0.1;
    worldSpace.y += cos(gTime + worldSpace.z + worldSpace.z) * 0.1;
    worldSpace.z += (sin(gTime + worldSpace.z) * 0.5 + cos(worldSpace.z + gTime)) * 0.1;

    gl_Position = gProjection * gView * worldSpace;
    vTexCoord = iTexCoord;
    vNormal = mat3(uTransform) * iNormal;
}

#endif

#ifdef RVO_FRAG

in vec2 vTexCoord;
in vec3 vNormal;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;

layout (binding = 0) uniform sampler2D tAlbedo;

void main(void) {
    oColor = texture(tAlbedo, vTexCoord);
    if (oColor.a < 0.7) discard;

    vec3 surfaceNormal = normalize(vNormal);

    if (!gl_FrontFacing) {
        surfaceNormal *= -1.0;
    }

    oNormal = vec4(surfaceNormal * 0.5 + 0.5, 1.0);
}

#endif