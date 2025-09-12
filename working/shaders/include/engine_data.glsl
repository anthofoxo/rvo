layout(std140, binding = 0) uniform EngineData {
	mat4 gView;
	mat4 gProjection;
	vec3 gSunDirection;
	float gFarPlane;
	float gTime;
};