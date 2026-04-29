#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <platform/platformInput.h>

struct Vertex3D
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
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

struct Shader
{
	GLuint program = 0;
	GLint u_mvp = -1;
	GLint u_model = -1;
	GLint u_lightDir = -1;
	GLint u_ambientStrength = -1;
	GLint u_vp = -1;
};

struct LineMesh
{
	GLuint vao = 0;
	GLuint vbo = 0;
	int vertexCount = 0;
};

struct FlyCamera
{
	glm::vec3 position = {0.f, 3.f, 10.f};
	float yaw = -90.f;
	float pitch = -15.f;
	float speed = 10.f;
	float sensitivity = 0.1f;
	bool active = false;

	int lastMouseX = 0;
	int lastMouseY = 0;
	bool firstMouse = true;

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix(float aspect) const;
	void update(platform::Input &input, float dt);
	glm::vec3 getFront() const;
};

Shader loadShader(const char *vertPath, const char *fragPath);
void destroyShader(Shader &shader);

Mesh createMesh(const std::vector<Vertex3D> &vertices, const std::vector<unsigned int> &indices = {});
Mesh loadGLB(const char *path);
void destroyMesh(Mesh &mesh);

LineMesh createLineMesh(const std::vector<LineVertex> &vertices);
void destroyLineMesh(LineMesh &mesh);

LineMesh createGridMesh(float size, float spacing);
LineMesh createAxisMesh(float length);

Mesh createGroundPlane(float size, glm::vec3 color);
Mesh createCube(glm::vec3 color);

void renderMesh(const Shader &shader, const Mesh &mesh, const glm::mat4 &model, const glm::mat4 &vp,
	const glm::vec3 &lightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.5f)), float ambient = 0.3f);
void renderLines(const Shader &shader, const LineMesh &mesh, const glm::mat4 &vp);
