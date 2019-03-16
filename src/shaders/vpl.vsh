#version 330 core
layout (location = 0) in vec3 aPos;

out float invalid;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform int debugVPLI = -1;

void main(){
	invalid = 0;
	if(debugVPLI != -1 && (gl_VertexID != debugVPLI * 2 && gl_VertexID != (debugVPLI * 2) + 1))
		invalid = 1;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
