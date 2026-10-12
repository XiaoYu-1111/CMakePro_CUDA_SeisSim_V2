#version 430 core
out vec2 texCoord3D;
// Generate a full-screen triangle without sharing 2-D geometry or vertex buffers.
void main() {
    vec2 position3D = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    texCoord3D = position3D;
    gl_Position = vec4(position3D * 2.0 - 1.0, 0.0, 1.0);
}
