#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 u_mvp;
uniform mat4 u_model;

out vec3 fragColor;
out vec3 fragNormal;

void main()
{
	gl_Position = u_mvp * vec4(aPos, 1.0);
	fragColor = aColor;
	fragNormal = mat3(transpose(inverse(u_model))) * aNormal;
}
