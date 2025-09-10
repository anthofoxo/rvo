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

uniform float uTime;

out float vVisibility;

void main(void) {
    vec4 worldSpace = uTransform * vec4(iPosition, 1.0);
    worldSpace.x += sin(uTime + worldSpace.x) * 0.1;
    worldSpace.y += cos(uTime + worldSpace.z + worldSpace.z) * 0.1;
    worldSpace.z += (sin(uTime + worldSpace.z) * 0.5 + cos(worldSpace.z + uTime)) * 0.1;

    vec4 eyeSpace = gView * worldSpace;

    gl_Position = gProjection * eyeSpace;
    vTexCoord = iTexCoord;
    vNormal = mat3(uTransform) * iNormal;

    float kDensity = 0.007;
    float kGradient = 1.5;

    float dist = length(eyeSpace.xyz);
    vVisibility= clamp(exp(-pow(dist * kDensity, kGradient)), 0.0, 1.0);

    
}

#endif

#ifdef RVO_FRAG

in float vVisibility;

in vec2 vTexCoord;
in vec3 vNormal;

layout (location = 0) out vec4 oColor;

layout (binding = 0) uniform sampler2D tAlbedo;

void main(void) {
    oColor = texture(tAlbedo, vTexCoord);
    if (oColor.a < 0.5) discard;

    vec3 surfaceNormal = normalize(vNormal);

    if (!gl_FrontFacing) {
        surfaceNormal *= -1.0;
    }

    oColor.rgb *= max(0.2, dot(surfaceNormal, vec3(0.0, 1.0, 0.0)));

    oColor.rgb = mix(vec3(pow(0.7, 2.2), pow(0.8, 2.2), pow(0.9, 2.2)), oColor.rgb, vVisibility);
}

#endif