#inject
#include "fullscreen.vert.glsl"

#ifdef RVO_FRAG
in vec2 texCoord;

layout (binding = 0) uniform sampler2D tColor;
layout (binding = 1) uniform sampler2D tBloom;

layout (location = 0) out vec4 oColor;

const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

const mat3 ACESOutputMat = mat3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFitted(vec3 color) {
    color = color * ACESInputMat;
    color = RRTAndODTFit(color);
    color = color * ACESOutputMat;
    color = clamp(color, 0.0, 1.0);
    return color;
}

uniform float uBlend;

void main(void) {
    vec4 colorSample = texture(tColor, texCoord);
    vec4 bloomSample = texture(tBloom, texCoord);
    vec4 finalSample = mix(colorSample, bloomSample, uBlend);
    oColor = vec4(ACESFitted(finalSample.rgb), 1.0);
    oColor.rgb = pow(oColor.rgb, vec3(1.0 / 2.2));
}
#endif