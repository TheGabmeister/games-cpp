#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include "animation.h"

struct Vertex3D
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

struct SkinnedVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::ivec4 boneIndices = {0, 0, 0, 0};
	glm::vec4 boneWeights = {1.f, 0.f, 0.f, 0.f};
};

struct LineVertex
{
	glm::vec3 position;
	glm::vec3 color;
};

struct Mesh
{
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;
	int indexCount = 0;
	int vertexCount = 0;
	bool indexed = false;
};

struct SkinnedMesh
{
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;
	int indexCount = 0;
	int vertexCount = 0;
	bool indexed = false;
};

struct SkinnedModel
{
	SkinnedMesh mesh;
	Skeleton skeleton;
	std::vector<AnimClip> clips;
	std::unordered_map<std::string, int> clipNameToIndex;
};

struct Shader
{
	GLuint program = 0;
	GLint u_mvp = -1;
	GLint u_model = -1;
	GLint u_lightDir = -1;
	GLint u_ambientStrength = -1;
	GLint u_vp = -1;
};

struct SkinnedShader
{
	GLuint program = 0;
	GLint u_mvp = -1;
	GLint u_model = -1;
	GLint u_lightDir = -1;
	GLint u_ambientStrength = -1;
	GLint u_bones[MAX_BONES] = {};
};

struct LineMesh
{
	GLuint vao = 0;
	GLuint vbo = 0;
	int vertexCount = 0;
};

Shader loadShader(const char *vertPath, const char *fragPath);
void destroyShader(Shader &shader);

SkinnedShader loadSkinnedShader(const char *vertPath, const char *fragPath);
void destroySkinnedShader(SkinnedShader &shader);

Mesh createMesh(const std::vector<Vertex3D> &vertices, const std::vector<unsigned int> &indices = {});
Mesh loadGLB(const char *path);
void destroyMesh(Mesh &mesh);

SkinnedMesh createSkinnedMesh(const std::vector<SkinnedVertex> &vertices, const std::vector<unsigned int> &indices = {});
SkinnedModel loadSkinnedGLB(const char *path);
void destroySkinnedMesh(SkinnedMesh &mesh);

LineMesh createLineMesh(const std::vector<LineVertex> &vertices);
void destroyLineMesh(LineMesh &mesh);

LineMesh createGridMesh(float size, float spacing);
LineMesh createAxisMesh(float length);

Mesh createGroundPlane(float size, glm::vec3 color);
Mesh createCube(glm::vec3 color);

void renderMesh(const Shader &shader, const Mesh &mesh, const glm::mat4 &model, const glm::mat4 &vp,
	const glm::vec3 &lightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.5f)), float ambient = 0.3f);
void renderSkinnedMesh(const SkinnedShader &shader, const SkinnedMesh &mesh,
	const glm::mat4 &model, const glm::mat4 &vp,
	const glm::mat4 *boneMatrices, int boneCount,
	const glm::vec3 &lightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.5f)), float ambient = 0.3f);
void renderLines(const Shader &shader, const LineMesh &mesh, const glm::mat4 &vp);
