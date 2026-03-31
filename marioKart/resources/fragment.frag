#version 330 core

in vec3 v_normal;

layout (location = 0) out vec4 color;

uniform vec3 u_color;

void main()
{
	vec3 lightDir = normalize(vec3(0.4, 0.8, 0.3));
	vec3 norm = normalize(v_normal);
	float ambient = 0.45;
	float diffuse = 0.55 * max(dot(norm, lightDir), 0.0);
	float brightness = ambient + diffuse;
	color = vec4(u_color * brightness, 1.0);
}
