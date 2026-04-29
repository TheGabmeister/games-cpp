#define GLM_ENABLE_EXPERIMENTAL
#include "renderer3d.h"
#include <glm/gtc/type_ptr.hpp>
#include <platformTools.h>
#include <logs.h>
#include <fstream>
#include <sstream>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

// ---- Shader ----

static std::string readFile(const char *path)
{
	std::ifstream f(path);
	if (!f.is_open())
	{
		platform::log((std::string("Failed to open file: ") + path).c_str());
		return "";
	}
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static GLuint compileShaderStage(const char *source, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		platform::log((std::string("Shader compilation error: ") + log).c_str());
	}
	return shader;
}

Shader loadShader(const char *vertPath, const char *fragPath)
{
	std::string vertSrc = readFile(vertPath);
	std::string fragSrc = readFile(fragPath);
	permaAssertComment(!vertSrc.empty(), "Vertex shader file empty or missing");
	permaAssertComment(!fragSrc.empty(), "Fragment shader file empty or missing");

	GLuint vert = compileShaderStage(vertSrc.c_str(), GL_VERTEX_SHADER);
	GLuint frag = compileShaderStage(fragSrc.c_str(), GL_FRAGMENT_SHADER);

	Shader s;
	s.program = glCreateProgram();
	glAttachShader(s.program, vert);
	glAttachShader(s.program, frag);
	glLinkProgram(s.program);

	GLint success;
	glGetProgramiv(s.program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char log[512];
		glGetProgramInfoLog(s.program, sizeof(log), nullptr, log);
		platform::log((std::string("Shader link error: ") + log).c_str());
	}

	glDeleteShader(vert);
	glDeleteShader(frag);

	s.u_mvp = glGetUniformLocation(s.program, "u_mvp");
	s.u_model = glGetUniformLocation(s.program, "u_model");
	s.u_lightDir = glGetUniformLocation(s.program, "u_lightDir");
	s.u_ambientStrength = glGetUniformLocation(s.program, "u_ambientStrength");
	s.u_vp = glGetUniformLocation(s.program, "u_vp");

	return s;
}

void destroyShader(Shader &shader)
{
	if (shader.program) glDeleteProgram(shader.program);
	shader = {};
}

// ---- Mesh ----

Mesh createMesh(const std::vector<Vertex3D> &vertices, const std::vector<unsigned int> &indices)
{
	Mesh m;
	m.vertexCount = (int)vertices.size();
	m.indexCount = (int)indices.size();
	m.indexed = !indices.empty();

	glGenVertexArrays(1, &m.vao);
	glGenBuffers(1, &m.vbo);
	glBindVertexArray(m.vao);

	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), vertices.data(), GL_STATIC_DRAW);

	if (m.indexed)
	{
		glGenBuffers(1, &m.ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	}

	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void *)offsetof(Vertex3D, position));
	glEnableVertexAttribArray(0);
	// normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void *)offsetof(Vertex3D, normal));
	glEnableVertexAttribArray(1);
	// color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void *)offsetof(Vertex3D, color));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
	return m;
}

void destroyMesh(Mesh &mesh)
{
	if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
	if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
	if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
	mesh = {};
}

// ---- Line Mesh ----

LineMesh createLineMesh(const std::vector<LineVertex> &vertices)
{
	LineMesh m;
	m.vertexCount = (int)vertices.size();

	glGenVertexArrays(1, &m.vao);
	glGenBuffers(1, &m.vbo);
	glBindVertexArray(m.vao);

	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LineVertex), vertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void *)offsetof(LineVertex, position));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void *)offsetof(LineVertex, color));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
	return m;
}

void destroyLineMesh(LineMesh &mesh)
{
	if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
	if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
	mesh = {};
}

// ---- Debug Geometry ----

LineMesh createGridMesh(float size, float spacing)
{
	std::vector<LineVertex> verts;
	glm::vec3 gray(0.45f, 0.45f, 0.45f);

	float half = size * 0.5f;
	int lines = (int)(size / spacing);

	for (int i = 0; i <= lines; i++)
	{
		float t = -half + i * spacing;
		// line along Z
		verts.push_back({{t, 0.f, -half}, gray});
		verts.push_back({{t, 0.f, half}, gray});
		// line along X
		verts.push_back({{-half, 0.f, t}, gray});
		verts.push_back({{half, 0.f, t}, gray});
	}

	return createLineMesh(verts);
}

LineMesh createAxisMesh(float length)
{
	std::vector<LineVertex> verts;
	glm::vec3 r(1, 0, 0), g(0, 1, 0), b(0, 0, 1);

	verts.push_back({{0, 0, 0}, r});
	verts.push_back({{length, 0, 0}, r});
	verts.push_back({{0, 0, 0}, g});
	verts.push_back({{0, length, 0}, g});
	verts.push_back({{0, 0, 0}, b});
	verts.push_back({{0, 0, length}, b});

	return createLineMesh(verts);
}

// ---- Primitive Geometry ----

Mesh createGroundPlane(float size, glm::vec3 color)
{
	float h = size * 0.5f;
	glm::vec3 n(0, 1, 0);
	std::vector<Vertex3D> verts = {
		{{-h, 0, -h}, n, color},
		{{-h, 0, h}, n, color},
		{{h, 0, h}, n, color},
		{{h, 0, -h}, n, color},
	};
	std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};
	return createMesh(verts, indices);
}

Mesh createCube(glm::vec3 color)
{
	std::vector<Vertex3D> verts;

	struct Face { glm::vec3 normal; glm::vec3 corners[4]; };
	Face faces[] = {
		// +Y (top)
		{{0,1,0}, {{-0.5f,0.5f,-0.5f},{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f},{0.5f,0.5f,-0.5f}}},
		// -Y (bottom)
		{{0,-1,0}, {{-0.5f,-0.5f,0.5f},{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f}}},
		// +X (right)
		{{1,0,0}, {{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{0.5f,0.5f,-0.5f}}},
		// -X (left)
		{{-1,0,0}, {{-0.5f,-0.5f,0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{-0.5f,0.5f,0.5f}}},
		// +Z (front)
		{{0,0,1}, {{-0.5f,-0.5f,0.5f},{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f},{0.5f,-0.5f,0.5f}}},
		// -Z (back)
		{{0,0,-1}, {{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{-0.5f,-0.5f,-0.5f}}},
	};

	std::vector<unsigned int> indices;
	for (auto &f : faces)
	{
		unsigned int base = (unsigned int)verts.size();
		for (int i = 0; i < 4; i++)
			verts.push_back({f.corners[i], f.normal, color});
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	}

	return createMesh(verts, indices);
}

// ---- glTF Loading ----

Mesh loadGLB(const char *path)
{
	cgltf_options options = {};
	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, path, &data);

	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to parse glTF: ") + path).c_str());
		return {};
	}

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to load glTF buffers: ") + path).c_str());
		cgltf_free(data);
		return {};
	}

	if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0)
	{
		platform::log((std::string("glTF has no meshes: ") + path).c_str());
		cgltf_free(data);
		return {};
	}

	cgltf_primitive &prim = data->meshes[0].primitives[0];

	cgltf_accessor *posAccessor = nullptr;
	cgltf_accessor *normAccessor = nullptr;
	cgltf_accessor *colorAccessor = nullptr;

	for (cgltf_size i = 0; i < prim.attributes_count; i++)
	{
		if (prim.attributes[i].type == cgltf_attribute_type_position) posAccessor = prim.attributes[i].data;
		if (prim.attributes[i].type == cgltf_attribute_type_normal) normAccessor = prim.attributes[i].data;
		if (prim.attributes[i].type == cgltf_attribute_type_color) colorAccessor = prim.attributes[i].data;
	}

	if (!posAccessor)
	{
		platform::log((std::string("glTF has no POSITION: ") + path).c_str());
		cgltf_free(data);
		return {};
	}

	size_t vertCount = posAccessor->count;
	std::vector<Vertex3D> vertices(vertCount);

	for (size_t i = 0; i < vertCount; i++)
	{
		cgltf_accessor_read_float(posAccessor, i, &vertices[i].position.x, 3);

		if (normAccessor)
			cgltf_accessor_read_float(normAccessor, i, &vertices[i].normal.x, 3);
		else
			vertices[i].normal = {0, 1, 0};

		if (colorAccessor)
		{
			float col[4] = {1, 1, 1, 1};
			cgltf_accessor_read_float(colorAccessor, i, col, colorAccessor->type == cgltf_type_vec4 ? 4 : 3);
			vertices[i].color = {col[0], col[1], col[2]};
		}
		else
		{
			vertices[i].color = {1, 1, 1};
		}
	}

	std::vector<unsigned int> indices;
	if (prim.indices)
	{
		indices.resize(prim.indices->count);
		for (size_t i = 0; i < prim.indices->count; i++)
			indices[i] = (unsigned int)cgltf_accessor_read_index(prim.indices, i);
	}

	cgltf_free(data);

	Mesh m = createMesh(vertices, indices);
	platform::log((std::string("Loaded GLB: ") + path + " (" + std::to_string(vertCount) + " verts, " + std::to_string(indices.size()) + " indices)").c_str());
	return m;
}

// ---- Rendering ----

void renderMesh(const Shader &shader, const Mesh &mesh, const glm::mat4 &model, const glm::mat4 &vp,
	const glm::vec3 &lightDir, float ambient)
{
	glUseProgram(shader.program);

	glm::mat4 mvp = vp * model;
	if (shader.u_mvp >= 0) glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	if (shader.u_model >= 0) glUniformMatrix4fv(shader.u_model, 1, GL_FALSE, glm::value_ptr(model));
	if (shader.u_lightDir >= 0) glUniform3fv(shader.u_lightDir, 1, glm::value_ptr(lightDir));
	if (shader.u_ambientStrength >= 0) glUniform1f(shader.u_ambientStrength, ambient);

	glBindVertexArray(mesh.vao);
	if (mesh.indexed)
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
	else
		glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
	glBindVertexArray(0);
}

void renderLines(const Shader &shader, const LineMesh &mesh, const glm::mat4 &vp)
{
	glUseProgram(shader.program);
	if (shader.u_vp >= 0) glUniformMatrix4fv(shader.u_vp, 1, GL_FALSE, glm::value_ptr(vp));

	glBindVertexArray(mesh.vao);
	glDrawArrays(GL_LINES, 0, mesh.vertexCount);
	glBindVertexArray(0);
}

// ---- FlyCamera ----

glm::vec3 FlyCamera::getFront() const
{
	float yawRad = glm::radians(yaw);
	float pitchRad = glm::radians(pitch);
	return glm::normalize(glm::vec3(
		cos(yawRad) * cos(pitchRad),
		sin(pitchRad),
		sin(yawRad) * cos(pitchRad)
	));
}

glm::mat4 FlyCamera::getViewMatrix() const
{
	glm::vec3 front = getFront();
	return glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
}

glm::mat4 FlyCamera::getProjectionMatrix(float aspect) const
{
	return glm::perspective(glm::radians(45.f), aspect, 0.1f, 500.f);
}

void FlyCamera::update(platform::Input &input, float dt)
{
	if (!active)
	{
		firstMouse = true;
		return;
	}

	// Mouse look
	int mx = input.mouseX;
	int my = input.mouseY;

	if (firstMouse)
	{
		lastMouseX = mx;
		lastMouseY = my;
		firstMouse = false;
	}

	float dx = (float)(mx - lastMouseX) * sensitivity;
	float dy = (float)(lastMouseY - my) * sensitivity;
	lastMouseX = mx;
	lastMouseY = my;

	yaw += dx;
	pitch += dy;
	if (pitch > 89.f) pitch = 89.f;
	if (pitch < -89.f) pitch = -89.f;

	// Movement
	glm::vec3 front = getFront();
	glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	glm::vec3 up(0, 1, 0);

	float moveSpeed = speed * dt;
	if (input.isButtonHeld(platform::Button::W)) position += front * moveSpeed;
	if (input.isButtonHeld(platform::Button::S)) position -= front * moveSpeed;
	if (input.isButtonHeld(platform::Button::A)) position -= right * moveSpeed;
	if (input.isButtonHeld(platform::Button::D)) position += right * moveSpeed;
	if (input.isButtonHeld(platform::Button::Space)) position += up * moveSpeed;
	if (input.isButtonHeld(platform::Button::LeftShift)) position -= up * moveSpeed;
}
