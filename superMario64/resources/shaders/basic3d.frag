#version 330 core

in vec3 fragColor;
in vec3 fragNormal;

uniform vec3 u_lightDir;
uniform float u_ambientStrength;
uniform float u_alpha = 1.0;

out vec4 outColor;

void main()
{
	vec3 normal = normalize(fragNormal);
	float diff = max(dot(normal, u_lightDir), 0.0);
	float lighting = u_ambientStrength + (1.0 - u_ambientStrength) * diff;
	outColor = vec4(fragColor * lighting, u_alpha);
}
