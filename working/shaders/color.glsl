#inject

#ifdef RVO_VERT

#include "engine_data.glsl"

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoord;

layout(location = 12) in mat4 uTransform;

out float vVisibility;

void main(void) {
    vec4 worldSpace = uTransform * vec4(iPosition, 1.0);
    vec4 eyeSpace = gView * worldSpace;
    gl_Position = gProjection * eyeSpace;

    float kDensity = 0.007;
    float kGradient = 1.5;

    float dist = length(eyeSpace.xyz);
    vVisibility= clamp(exp(-pow(dist * kDensity, kGradient)), 0.0, 1.0);
}

#endif

#ifdef RVO_FRAG

layout (location = 0) out vec4 oColor;

uniform vec3 uColor;

in float vVisibility;

void main(void) {
    oColor = vec4(uColor, 1.0);
    oColor.rgb = mix(vec3(pow(0.7, 2.2), pow(0.8, 2.2), pow(0.9, 2.2)), oColor.rgb, vVisibility);
}

#endif