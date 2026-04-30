#define GLM_ENABLE_EXPERIMENTAL
#include "renderer.h"
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

SkinnedShader loadSkinnedShader(const char *vertPath, const char *fragPath)
{
	std::string vertSrc = readFile(vertPath);
	std::string fragSrc = readFile(fragPath);
	permaAssertComment(!vertSrc.empty(), "Skinned vertex shader file empty or missing");
	permaAssertComment(!fragSrc.empty(), "Skinned fragment shader file empty or missing");

	GLuint vert = compileShaderStage(vertSrc.c_str(), GL_VERTEX_SHADER);
	GLuint frag = compileShaderStage(fragSrc.c_str(), GL_FRAGMENT_SHADER);

	SkinnedShader s;
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
		platform::log((std::string("Skinned shader link error: ") + log).c_str());
	}

	glDeleteShader(vert);
	glDeleteShader(frag);

	s.u_mvp = glGetUniformLocation(s.program, "u_mvp");
	s.u_model = glGetUniformLocation(s.program, "u_model");
	s.u_lightDir = glGetUniformLocation(s.program, "u_lightDir");
	s.u_ambientStrength = glGetUniformLocation(s.program, "u_ambientStrength");

	for (int i = 0; i < MAX_BONES; i++)
	{
		std::string name = "u_bones[" + std::to_string(i) + "]";
		s.u_bones[i] = glGetUniformLocation(s.program, name.c_str());
	}

	return s;
}

void destroySkinnedShader(SkinnedShader &shader)
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

// ---- Skinned Mesh ----

SkinnedMesh createSkinnedMesh(const std::vector<SkinnedVertex> &vertices, const std::vector<unsigned int> &indices)
{
	SkinnedMesh m;
	m.vertexCount = (int)vertices.size();
	m.indexCount = (int)indices.size();
	m.indexed = !indices.empty();

	glGenVertexArrays(1, &m.vao);
	glGenBuffers(1, &m.vbo);
	glBindVertexArray(m.vao);

	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex), vertices.data(), GL_STATIC_DRAW);

	if (m.indexed)
	{
		glGenBuffers(1, &m.ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	}

	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void *)offsetof(SkinnedVertex, position));
	glEnableVertexAttribArray(0);
	// normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void *)offsetof(SkinnedVertex, normal));
	glEnableVertexAttribArray(1);
	// color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void *)offsetof(SkinnedVertex, color));
	glEnableVertexAttribArray(2);
	// bone indices (integer attribute)
	glVertexAttribIPointer(3, 4, GL_INT, sizeof(SkinnedVertex), (void *)offsetof(SkinnedVertex, boneIndices));
	glEnableVertexAttribArray(3);
	// bone weights
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void *)offsetof(SkinnedVertex, boneWeights));
	glEnableVertexAttribArray(4);

	glBindVertexArray(0);
	return m;
}

void destroySkinnedMesh(SkinnedMesh &mesh)
{
	if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
	if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
	if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
	mesh = {};
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

// ---- Skinned glTF Loading ----

static int findNodeIndex(cgltf_data *data, cgltf_node *node)
{
	for (cgltf_size i = 0; i < data->nodes_count; i++)
		if (&data->nodes[i] == node) return (int)i;
	return -1;
}

static int findJointIndex(cgltf_skin *skin, cgltf_node *node)
{
	for (cgltf_size i = 0; i < skin->joints_count; i++)
		if (skin->joints[i] == node) return (int)i;
	return -1;
}

SkinnedModel loadSkinnedGLB(const char *path)
{
	SkinnedModel model;

	cgltf_options options = {};
	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to parse skinned glTF: ") + path).c_str());
		return model;
	}

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to load skinned glTF buffers: ") + path).c_str());
		cgltf_free(data);
		return model;
	}

	// Find the mesh node with a skin
	cgltf_mesh *mesh = nullptr;
	cgltf_skin *skin = nullptr;
	for (cgltf_size i = 0; i < data->nodes_count; i++)
	{
		if (data->nodes[i].mesh && data->nodes[i].skin)
		{
			mesh = data->nodes[i].mesh;
			skin = data->nodes[i].skin;
			break;
		}
	}

	if (!mesh || mesh->primitives_count == 0)
	{
		platform::log((std::string("Skinned glTF has no skinned mesh: ") + path).c_str());
		cgltf_free(data);
		return model;
	}

	if (!skin || skin->joints_count == 0)
	{
		platform::log((std::string("Skinned glTF has no skin/joints: ") + path).c_str());
		cgltf_free(data);
		return model;
	}

	// Build skeleton
	int boneCount = (int)skin->joints_count;
	model.skeleton.bones.resize(boneCount);

	// Read inverse bind matrices
	std::vector<glm::mat4> ibms(boneCount, glm::mat4(1.f));
	if (skin->inverse_bind_matrices)
	{
		for (int i = 0; i < boneCount; i++)
			cgltf_accessor_read_float(skin->inverse_bind_matrices, i, &ibms[i][0][0], 16);
	}

	for (int i = 0; i < boneCount; i++)
	{
		cgltf_node *joint = skin->joints[i];
		Bone &bone = model.skeleton.bones[i];

		bone.name = joint->name ? joint->name : ("bone_" + std::to_string(i));
		bone.inverseBindMatrix = ibms[i];

		// Find parent in joint list
		bone.parentIndex = -1;
		if (joint->parent)
			bone.parentIndex = findJointIndex(skin, joint->parent);

		// Bind pose local transform
		if (joint->has_translation)
			bone.localPosition = {joint->translation[0], joint->translation[1], joint->translation[2]};
		if (joint->has_rotation)
			bone.localRotation = glm::quat(joint->rotation[3], joint->rotation[0], joint->rotation[1], joint->rotation[2]);
		if (joint->has_scale)
			bone.localScale = {joint->scale[0], joint->scale[1], joint->scale[2]};

		model.skeleton.boneNameToIndex[bone.name] = i;
	}

	// Read mesh vertices
	cgltf_primitive &prim = mesh->primitives[0];

	cgltf_accessor *posAccessor = nullptr;
	cgltf_accessor *normAccessor = nullptr;
	cgltf_accessor *colorAccessor = nullptr;
	cgltf_accessor *jointsAccessor = nullptr;
	cgltf_accessor *weightsAccessor = nullptr;

	for (cgltf_size i = 0; i < prim.attributes_count; i++)
	{
		switch (prim.attributes[i].type)
		{
		case cgltf_attribute_type_position: posAccessor = prim.attributes[i].data; break;
		case cgltf_attribute_type_normal: normAccessor = prim.attributes[i].data; break;
		case cgltf_attribute_type_color: colorAccessor = prim.attributes[i].data; break;
		case cgltf_attribute_type_joints: jointsAccessor = prim.attributes[i].data; break;
		case cgltf_attribute_type_weights: weightsAccessor = prim.attributes[i].data; break;
		default: break;
		}
	}

	if (!posAccessor)
	{
		platform::log((std::string("Skinned glTF has no POSITION: ") + path).c_str());
		cgltf_free(data);
		return model;
	}

	size_t vertCount = posAccessor->count;
	std::vector<SkinnedVertex> vertices(vertCount);

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

		if (jointsAccessor)
		{
			cgltf_uint joints[4] = {0, 0, 0, 0};
			cgltf_accessor_read_uint(jointsAccessor, i, joints, 4);
			vertices[i].boneIndices = {(int)joints[0], (int)joints[1], (int)joints[2], (int)joints[3]};
		}

		if (weightsAccessor)
		{
			cgltf_accessor_read_float(weightsAccessor, i, &vertices[i].boneWeights.x, 4);
		}
	}

	std::vector<unsigned int> indices;
	if (prim.indices)
	{
		indices.resize(prim.indices->count);
		for (size_t i = 0; i < prim.indices->count; i++)
			indices[i] = (unsigned int)cgltf_accessor_read_index(prim.indices, i);
	}

	model.mesh = createSkinnedMesh(vertices, indices);

	// Read animations
	for (cgltf_size a = 0; a < data->animations_count; a++)
	{
		cgltf_animation &anim = data->animations[a];
		AnimClip clip;
		clip.name = anim.name ? anim.name : ("anim_" + std::to_string(a));
		clip.duration = 0.f;
		clip.looping = false;

		for (cgltf_size c = 0; c < anim.channels_count; c++)
		{
			cgltf_animation_channel &ch = anim.channels[c];
			if (!ch.target_node) continue;

			int boneIdx = findJointIndex(skin, ch.target_node);
			if (boneIdx < 0) continue;

			AnimChannel channel;
			channel.boneIndex = boneIdx;

			switch (ch.target_path)
			{
			case cgltf_animation_path_type_translation: channel.path = AnimChannel::Path::TRANSLATION; break;
			case cgltf_animation_path_type_rotation: channel.path = AnimChannel::Path::ROTATION; break;
			case cgltf_animation_path_type_scale: channel.path = AnimChannel::Path::SCALE; break;
			default: continue;
			}

			cgltf_animation_sampler *sampler = ch.sampler;
			cgltf_accessor *input = sampler->input;
			cgltf_accessor *output = sampler->output;

			size_t keyCount = input->count;
			channel.keyframes.resize(keyCount);

			for (size_t k = 0; k < keyCount; k++)
			{
				float t = 0.f;
				cgltf_accessor_read_float(input, k, &t, 1);
				channel.keyframes[k].time = t;
				if (t > clip.duration) clip.duration = t;

				if (channel.path == AnimChannel::Path::ROTATION)
				{
					float q[4];
					cgltf_accessor_read_float(output, k, q, 4);
					channel.keyframes[k].value4 = glm::quat(q[3], q[0], q[1], q[2]);
				}
				else
				{
					cgltf_accessor_read_float(output, k, &channel.keyframes[k].value3.x, 3);
				}
			}

			clip.channels.push_back(std::move(channel));
		}

		// Mark looping clips by name convention
		if (clip.name == "idle" || clip.name == "walk" || clip.name == "run" ||
		    clip.name == "freefall" || clip.name == "crawl" || clip.name == "belly_slide" ||
		    clip.name == "pole_climb" || clip.name == "carry_idle" || clip.name == "carry_walk")
			clip.looping = true;

		model.clipNameToIndex[clip.name] = (int)model.clips.size();
		model.clips.push_back(std::move(clip));
	}

	cgltf_free(data);

	platform::log((std::string("Loaded skinned GLB: ") + path +
	               " (" + std::to_string(vertCount) + " verts, " +
	               std::to_string(boneCount) + " bones, " +
	               std::to_string(model.clips.size()) + " clips)").c_str());
	return model;
}

// ---- Rendering ----

void renderSkinnedMesh(const SkinnedShader &shader, const SkinnedMesh &mesh,
	const glm::mat4 &model, const glm::mat4 &vp,
	const glm::mat4 *boneMatrices, int boneCount,
	const glm::vec3 &lightDir, float ambient)
{
	glUseProgram(shader.program);

	glm::mat4 mvp = vp * model;
	if (shader.u_mvp >= 0) glUniformMatrix4fv(shader.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	if (shader.u_model >= 0) glUniformMatrix4fv(shader.u_model, 1, GL_FALSE, glm::value_ptr(model));
	if (shader.u_lightDir >= 0) glUniform3fv(shader.u_lightDir, 1, glm::value_ptr(lightDir));
	if (shader.u_ambientStrength >= 0) glUniform1f(shader.u_ambientStrength, ambient);

	int count = std::min(boneCount, MAX_BONES);
	for (int i = 0; i < count; i++)
	{
		if (shader.u_bones[i] >= 0)
			glUniformMatrix4fv(shader.u_bones[i], 1, GL_FALSE, glm::value_ptr(boneMatrices[i]));
	}

	glBindVertexArray(mesh.vao);
	if (mesh.indexed)
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
	else
		glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
	glBindVertexArray(0);
}

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
