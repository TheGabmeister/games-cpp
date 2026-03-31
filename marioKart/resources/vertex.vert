#version 330 core

layout (location = 0) in vec3 p;
layout (location = 1) in vec3 n;

uniform mat4 u_mat;
uniform mat4 u_model;

out vec3 v_normal;

void main()
{
	gl_Position = u_mat * vec4(p, 1);
	v_normal = mat3(u_model) * n;
}
