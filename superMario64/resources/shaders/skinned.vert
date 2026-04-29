#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in ivec4 aBoneIndices;
layout (location = 4) in vec4 aBoneWeights;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_bones[64];

out vec3 fragColor;
out vec3 fragNormal;

void main()
{
	mat4 skin = u_bones[aBoneIndices.x] * aBoneWeights.x
	          + u_bones[aBoneIndices.y] * aBoneWeights.y
	          + u_bones[aBoneIndices.z] * aBoneWeights.z
	          + u_bones[aBoneIndices.w] * aBoneWeights.w;

	vec4 skinnedPos = skin * vec4(aPos, 1.0);
	vec3 skinnedNormal = mat3(skin) * aNormal;

	gl_Position = u_mvp * skinnedPos;
	fragColor = aColor;
	fragNormal = mat3(transpose(inverse(u_model))) * skinnedNormal;
}
