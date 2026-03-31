#include "renderer.h"
#include "gameState.h"
#include "gameLayer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <logs.h>
#include <cstdio>

namespace
{
	GLuint shaderProgram = 0;
	GLint uMatLocation = -1;
	GLint uModelLocation = -1;
	GLint uColorLocation = -1;

	GLuint cubeVAO = 0, cubeVBO = 0;
	GLuint quadVAO = 0, quadVBO = 0;
	GLuint lineVAO = 0, lineVBO = 0;

	glm::mat4 viewProjection = glm::mat4(1.f);

	// clang-format off
	// Interleaved: position (vec3) + normal (vec3) per vertex
	const float cubeVertices[] = {
		// Front face (z = 0.5), normal = (0, 0, 1)
		-0.5f, -0.5f,  0.5f,  0,0,1,   0.5f, -0.5f,  0.5f,  0,0,1,   0.5f,  0.5f,  0.5f,  0,0,1,
		-0.5f, -0.5f,  0.5f,  0,0,1,   0.5f,  0.5f,  0.5f,  0,0,1,  -0.5f,  0.5f,  0.5f,  0,0,1,
		// Back face (z = -0.5), normal = (0, 0, -1)
		 0.5f, -0.5f, -0.5f,  0,0,-1, -0.5f, -0.5f, -0.5f,  0,0,-1, -0.5f,  0.5f, -0.5f,  0,0,-1,
		 0.5f, -0.5f, -0.5f,  0,0,-1, -0.5f,  0.5f, -0.5f,  0,0,-1,  0.5f,  0.5f, -0.5f,  0,0,-1,
		// Left face (x = -0.5), normal = (-1, 0, 0)
		-0.5f, -0.5f, -0.5f, -1,0,0,  -0.5f, -0.5f,  0.5f, -1,0,0,  -0.5f,  0.5f,  0.5f, -1,0,0,
		-0.5f, -0.5f, -0.5f, -1,0,0,  -0.5f,  0.5f,  0.5f, -1,0,0,  -0.5f,  0.5f, -0.5f, -1,0,0,
		// Right face (x = 0.5), normal = (1, 0, 0)
		 0.5f, -0.5f,  0.5f,  1,0,0,   0.5f, -0.5f, -0.5f,  1,0,0,   0.5f,  0.5f, -0.5f,  1,0,0,
		 0.5f, -0.5f,  0.5f,  1,0,0,   0.5f,  0.5f, -0.5f,  1,0,0,   0.5f,  0.5f,  0.5f,  1,0,0,
		// Top face (y = 0.5), normal = (0, 1, 0)
		-0.5f,  0.5f,  0.5f,  0,1,0,   0.5f,  0.5f,  0.5f,  0,1,0,   0.5f,  0.5f, -0.5f,  0,1,0,
		-0.5f,  0.5f,  0.5f,  0,1,0,   0.5f,  0.5f, -0.5f,  0,1,0,  -0.5f,  0.5f, -0.5f,  0,1,0,
		// Bottom face (y = -0.5), normal = (0, -1, 0)
		-0.5f, -0.5f, -0.5f,  0,-1,0,  0.5f, -0.5f, -0.5f,  0,-1,0,  0.5f, -0.5f,  0.5f,  0,-1,0,
		-0.5f, -0.5f, -0.5f,  0,-1,0,  0.5f, -0.5f,  0.5f,  0,-1,0, -0.5f, -0.5f,  0.5f,  0,-1,0,
	};

	// Quad: flat on XZ plane, normal = (0, 1, 0)
	const float quadVertices[] = {
		-0.5f, 0.f, -0.5f,  0,1,0,   0.5f, 0.f, -0.5f,  0,1,0,   0.5f, 0.f,  0.5f,  0,1,0,
		-0.5f, 0.f, -0.5f,  0,1,0,   0.5f, 0.f,  0.5f,  0,1,0,  -0.5f, 0.f,  0.5f,  0,1,0,
	};
	// clang-format on

	GLuint compileShader(GLenum type, const char *source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char info[512] = {};
			glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
			char msg[600] = {};
			std::snprintf(msg, sizeof(msg), "Shader compile error: %s", info);
			platform::log(msg, LogManager::logError);
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	void createVAO(GLuint &vao, GLuint &vbo, const float *data, size_t dataSize, GLenum usage, bool hasNormals = true)
	{
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, dataSize, data, usage);

		if (hasNormals)
		{
			GLsizei stride = 6 * sizeof(float);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);
		}
		else
		{
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
			glEnableVertexAttribArray(0);
		}

		glBindVertexArray(0);
	}

	void setUniforms(const glm::mat4 &mvp, const glm::mat4 &model, const glm::vec3 &color)
	{
		glUniformMatrix4fv(uMatLocation, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniformMatrix4fv(uModelLocation, 1, GL_FALSE, glm::value_ptr(model));
		glUniform3fv(uColorLocation, 1, glm::value_ptr(color));
	}
}

namespace renderer
{
	bool init()
	{
		bool vsOk = false, fsOk = false;
		std::string vsSrc = platform::readEntireFile(RESOURCES_PATH "vertex.vert", &vsOk);
		std::string fsSrc = platform::readEntireFile(RESOURCES_PATH "fragment.frag", &fsOk);

		if (!vsOk || !fsOk)
		{
			platform::log("Failed to load shader files", LogManager::logError);
			return false;
		}

		GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc.c_str());
		GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc.c_str());
		if (!vs || !fs)
		{
			if (vs) { glDeleteShader(vs); }
			if (fs) { glDeleteShader(fs); }
			return false;
		}

		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vs);
		glAttachShader(shaderProgram, fs);
		glLinkProgram(shaderProgram);

		GLint success = 0;
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
		if (!success)
		{
			char info[512] = {};
			glGetProgramInfoLog(shaderProgram, sizeof(info), nullptr, info);
			char msg[600] = {};
			std::snprintf(msg, sizeof(msg), "Shader link error: %s", info);
			platform::log(msg, LogManager::logError);
		}

		glDeleteShader(vs);
		glDeleteShader(fs);

		if (!success) { return false; }

		uMatLocation = glGetUniformLocation(shaderProgram, "u_mat");
		uModelLocation = glGetUniformLocation(shaderProgram, "u_model");
		uColorLocation = glGetUniformLocation(shaderProgram, "u_color");

		createVAO(cubeVAO, cubeVBO, cubeVertices, sizeof(cubeVertices), GL_STATIC_DRAW, true);
		createVAO(quadVAO, quadVBO, quadVertices, sizeof(quadVertices), GL_STATIC_DRAW, true);
		createVAO(lineVAO, lineVBO, nullptr, 6 * sizeof(float), GL_DYNAMIC_DRAW, false);

		platform::log("Renderer initialized");
		return true;
	}

	void close()
	{
		if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
		if (cubeVBO) { glDeleteBuffers(1, &cubeVBO); cubeVBO = 0; }
		if (cubeVAO) { glDeleteVertexArrays(1, &cubeVAO); cubeVAO = 0; }
		if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
		if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
		if (lineVBO) { glDeleteBuffers(1, &lineVBO); lineVBO = 0; }
		if (lineVAO) { glDeleteVertexArrays(1, &lineVAO); lineVAO = 0; }
		platform::log("Renderer closed");
	}

	void beginFrame(const CameraState &camera, int viewportWidth, int viewportHeight)
	{
		float aspect = (viewportHeight > 0) ? static_cast<float>(viewportWidth) / viewportHeight : 1.f;
		glm::mat4 projection = glm::perspective(glm::radians(65.f), aspect, 0.1f, 200.f);
		glm::mat4 view = glm::lookAt(camera.position, camera.target, glm::vec3(0.f, 1.f, 0.f));
		viewProjection = projection * view;

		glUseProgram(shaderProgram);
		glEnable(GL_DEPTH_TEST);
	}

	void drawQuad(glm::vec3 center, glm::vec2 size, glm::vec3 color, float rotationY)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.f), center);
		if (rotationY != 0.f)
		{
			model = glm::rotate(model, rotationY, glm::vec3(0.f, 1.f, 0.f));
		}
		model = glm::scale(model, glm::vec3(size.x, 1.f, size.y));

		setUniforms(viewProjection * model, model, color);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void drawBox(glm::vec3 center, glm::vec3 size, glm::vec3 color, float rotationY)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.f), center);
		if (rotationY != 0.f)
		{
			model = glm::rotate(model, rotationY, glm::vec3(0.f, 1.f, 0.f));
		}
		model = glm::scale(model, size);

		setUniforms(viewProjection * model, model, color);
		glBindVertexArray(cubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	void drawLine(glm::vec3 start, glm::vec3 end, glm::vec3 color)
	{
		float verts[6] = {start.x, start.y, start.z, end.x, end.y, end.z};

		glBindVertexArray(lineVAO);
		glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

		setUniforms(viewProjection, glm::mat4(1.f), color);
		glDrawArrays(GL_LINES, 0, 2);
	}

	void drawMarker(glm::vec3 position, float size, glm::vec3 color)
	{
		float half = size * 0.5f;
		drawLine(position - glm::vec3(half, 0.f, 0.f), position + glm::vec3(half, 0.f, 0.f), color);
		drawLine(position - glm::vec3(0.f, half, 0.f), position + glm::vec3(0.f, half, 0.f), color);
		drawLine(position - glm::vec3(0.f, 0.f, half), position + glm::vec3(0.f, 0.f, half), color);
	}
}
