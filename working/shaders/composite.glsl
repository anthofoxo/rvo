#inject
#include "fullscreen.vert.glsl"
#include "engine_data.glsl"

#ifdef RVO_FRAG
in vec2 texCoord;

layout (binding = 0) uniform sampler2D tAlbedo;
layout (binding = 1) uniform sampler2D tNormal;
layout (binding = 2) uniform sampler2D tDepth;

layout (location = 0) out vec4 oColor;

// transpose(inverse(mat3(matrix))) * normal

vec4 get_eye_space(vec2 aUv, float aDepthSample) {
	vec4 clipSpace = vec4(vec3(aUv, aDepthSample) * 2.0 - 1.0, 1.0);
	vec4 eyeSpace = inverse(gProjection) * clipSpace;
	return eyeSpace / eyeSpace.w;
}

const float kDensity = 0.007;
const float kGradient = 1.5;
const vec3 kSkyColor = pow(vec3(0.7, 0.8, 0.9), vec3(2.2));

void main(void) {
    vec4 colorSample = texture(tAlbedo, texCoord);
    vec4 normalSample = texture(tNormal, texCoord);
    normalSample.xyz = normalSample.xyz * 2.0 - 1.0;

    float depthSample = texture(tDepth, texCoord).x;
    float dist = length(get_eye_space(texCoord, depthSample).xyz);
    //float visibility = clamp(exp(-pow(dist * kDensity, kGradient)), 0.0, 1.0);
    
    float diffuseBrightness = max(0.2, dot(normalSample.xyz, -gSunDirection));
    // If no normal is was written then treat this fragment as fully lit
    diffuseBrightness = mix(1.0, diffuseBrightness, normalSample.a);
    
    oColor = vec4(colorSample.rgb * diffuseBrightness, 1.0);

    //oColor.rgb = mix(kSkyColor, oColor.rgb, visibility);

    oColor.rgb = mix(oColor.rgb, kSkyColor, smoothstep(0.0, gFarPlane, dist));
}
#endif