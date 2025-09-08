#inject
#include "fullscreen.vert.glsl"

#ifdef RVO_FRAG
// This shader performs downsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
layout (binding = 0) uniform sampler2D srcTexture;
uniform vec2 srcResolution;

in vec2 texCoord;
layout (location = 0) out vec3 downsample;

uniform int uBasePass;

vec3 PowVec3(vec3 v, float p) {
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float invGamma = 1.0 / 2.2;
vec3 ToSRGB(vec3 v) { return PowVec3(v, invGamma); }

float RGBToLuminance(vec3 col) {
    return dot(col, vec3(0.2126, 0.7152, 0.0722));
}

float KarisAverage(vec3 col) {
    float luma = RGBToLuminance(ToSRGB(col)) * 0.25;
    return 1.0 / (1.0 + luma);
}

void main(void) {
    vec2 srcTexelSize = 1.0 / srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(srcTexture, vec2(texCoord.x - 2.0 * x, texCoord.y + 2.0 * y)).rgb;
    vec3 b = texture(srcTexture, vec2(texCoord.x,           texCoord.y + 2.0 * y)).rgb;
    vec3 c = texture(srcTexture, vec2(texCoord.x + 2.0 * x, texCoord.y + 2.0 * y)).rgb;

    vec3 d = texture(srcTexture, vec2(texCoord.x - 2.0 * x, texCoord.y)).rgb;
    vec3 e = texture(srcTexture, vec2(texCoord.x,           texCoord.y)).rgb;
    vec3 f = texture(srcTexture, vec2(texCoord.x + 2.0 * x, texCoord.y)).rgb;

    vec3 g = texture(srcTexture, vec2(texCoord.x - 2.0 * x, texCoord.y - 2.0 * y)).rgb;
    vec3 h = texture(srcTexture, vec2(texCoord.x,           texCoord.y - 2.0 * y)).rgb;
    vec3 i = texture(srcTexture, vec2(texCoord.x + 2.0 * x, texCoord.y - 2.0 * y)).rgb;

    vec3 j = texture(srcTexture, vec2(texCoord.x - x, texCoord.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(texCoord.x + x, texCoord.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(texCoord.x - x, texCoord.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(texCoord.x + x, texCoord.y - y)).rgb;

    if (uBasePass == 1) {
        //#define REMOVE_NON_HDR
        #ifdef REMOVE_NON_HDR
        if (RGBToLuminance(a) < 1.0) a = vec3(0.0);
        if (RGBToLuminance(b) < 1.0) b = vec3(0.0);
        if (RGBToLuminance(c) < 1.0) c = vec3(0.0);
        if (RGBToLuminance(d) < 1.0) d = vec3(0.0);
        if (RGBToLuminance(e) < 1.0) e = vec3(0.0);
        if (RGBToLuminance(f) < 1.0) f = vec3(0.0);
        if (RGBToLuminance(g) < 1.0) g = vec3(0.0);
        if (RGBToLuminance(h) < 1.0) h = vec3(0.0);
        if (RGBToLuminance(i) < 1.0) i = vec3(0.0);
        if (RGBToLuminance(j) < 1.0) j = vec3(0.0);
        if (RGBToLuminance(k) < 1.0) k = vec3(0.0);
        if (RGBToLuminance(l) < 1.0) l = vec3(0.0);
        if (RGBToLuminance(m) < 1.0) m = vec3(0.0);
        #endif

        vec3 groups[5];
        groups[0] = (a + b + d + e) / 4.0;
        groups[1] = (b + c + e + f) / 4.0;
        groups[2] = (d + e + g + h) / 4.0;
        groups[3] = (e + f + h + i) / 4.0;
        groups[4] = (j + k + l + m) / 4.0;
        float kw0 = KarisAverage(groups[0]);
        float kw1 = KarisAverage(groups[1]);
        float kw2 = KarisAverage(groups[2]);
        float kw3 = KarisAverage(groups[3]);
        float kw4 = KarisAverage(groups[4]);
        downsample = (kw0 * groups[0] + kw1 * groups[1] + kw2 * groups[2] + kw3 * groups[3] + kw4 * groups[4]) / (kw0 + kw1 + kw2 + kw3 + kw4);
    }
    else {
        downsample = e * 0.125;
        downsample += (a + c + g + i) * 0.03125;
        downsample += (b + d + f + h) * 0.0625;
        downsample += (j + k + l + m) * 0.125;
    }    
}
#endif