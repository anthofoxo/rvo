#ifdef RVO_VERT
out vec2 texCoord;

void main(void) {
    texCoord = vec2(gl_VertexID & 2, 1 - ((gl_VertexID << 1) & 2));
	gl_Position = vec4(texCoord * 2.0 - 1.0, 0.0, 1.0);
}
#endif