// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <ctime>
#include <iostream>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>
#include <common/objloader.hpp>

using vec3s = std::vector<vec3>;
using vec2s = std::vector<vec2>;

float distance(const vec3& a, const vec3& b) {
	float ans = 0;
	vec3 delta = a - b;
	for (int i = 0; i < 3; ++i) {
		ans += delta[i] * delta[i];
	}
	return sqrt(ans);
}

vec3& operator*=(vec3& a, const int n) {
	for (int i = 0; i < 3; ++i) {
		a[i] *= n;
	}
	return a;
}

vec3 operator*(vec3 a, const int n) {
	return a *= n;
}

vec3& operator/=(vec3& a, const int n) {
	for (int i = 0; i < 3; ++i) {
		a[i] /= n;
	}
	return a;
}

vec3 operator/(vec3 a, const int n) {
	return a /= n;
}

class Object {
public:
	vec3s vertices;
	vec2s uvs;
	vec3s normals;

	vec3 pseudo_center = { 0.f, 0.f, 0.f };
	float pseudo_radius = 0;

	GLuint vertex_array_id = -1;
	GLuint vertex_buffer_id = -1;
	GLuint uv_buffer_id = -1;
	GLuint texture_id = -1;

	bool destroyed = false;
	vec3 velocity{0,0,0};

	static GLuint uniform_texture_id;
	static GLuint uniform_matrix_mvp;

	Object() {
		glGenVertexArrays(1, &vertex_array_id);
		glGenBuffers(1, &vertex_buffer_id);
		glGenBuffers(1, &uv_buffer_id);
	}

	Object(vec3s vertices, vec2s uvs, vec3s normals) :
		vertices(vertices), uvs(uvs), normals(normals) {
		init();
		glGenVertexArrays(1, &vertex_array_id);
		glGenBuffers(1, &vertex_buffer_id);
		glGenBuffers(1, &uv_buffer_id);
	}

	void load() {
		glBindVertexArray(vertex_array_id);

		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		glBindBuffer(GL_ARRAY_BUFFER, uv_buffer_id);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);
	}

	void init() {
		int n = vertices.size();
		for (auto& vertex : vertices) {
			vertex /= 15;
		}
		update_center();
		pseudo_radius = 0;
		for (const auto& vertex : vertices) {
			pseudo_radius += distance(pseudo_center, vertex);
		}
		pseudo_radius /= n;
		pseudo_radius = sqrt(pseudo_radius);
	}

	void update_center() {
		int n = vertices.size();
		pseudo_center = { 0.f, 0.f, 0.f };
		for (auto& vertex : vertices) {
			pseudo_center += vertex;
		}
		pseudo_center /= n;
	}

	void render() {
		load();
		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniform1i(uniform_texture_id, texture_id);
		glBindVertexArray(vertex_array_id);
		glUniformMatrix4fv(uniform_matrix_mvp, 1, GL_FALSE, &MVP[0][0]);
		glDrawArrays(GL_TRIANGLES, 0, vertices.size());
	}

	void move(const vec3& displacement) {
		for (auto& vertex : vertices) {
			vertex += displacement;
		}
		update_center();
	}

	void move(float delta_time) {
		move(velocity * delta_time);
	}
	
	/*~Object() {
		glDeleteBuffers(1, &vertex_buffer_id);
		glDeleteBuffers(1, &uv_buffer_id);
	}*/
};

GLuint Object::uniform_texture_id;
GLuint Object::uniform_matrix_mvp;

using Objects = std::vector<Object>;

float rand_float() {
	return static_cast<float>(4 * rand() / RAND_MAX);
}

bool are_close(vec3 obj1_center, vec3 obj2_center, float sum_of_radii) {
	vec3 x;
	for (int i = 0; i < 3; ++i)	x[i] = obj1_center[i] - obj2_center[i];
	return x[0] * x[0] + x[1] * x[1] + x[2] * x[2] < sum_of_radii * sum_of_radii;
}

void gl_init() {
	srand(time(NULL));

	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		exit(-1);
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Tutorial 07 - Model Loading", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		exit(-1);
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	// Hide the mouse and enable unlimited mouvement
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set the mouse at the center of the screen
	glfwPollEvents();
	glfwSetCursorPos(window, 1024 / 2, 768 / 2);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);
}

void update(Objects& v, float delta_time) {
	for (int i = 0; i < v.size(); ) {
		v[i].move(delta_time);
		if (v[i].destroyed) {
			std::swap(v[i], v.back());
			v.pop_back();
		}
		else {
			v[i].render();
			++i;
		}
	}
}

int main()
{
	gl_init();

	GLuint programID = LoadShaders("TransformVertexShader.vertexshader.glsl", "TextureFragmentShader.fragmentshader.glsl");
	glUseProgram(programID);

	GLuint Texture = loadDDS("aau1t-25se1.DDS");
	Object::uniform_texture_id = glGetUniformLocation(programID, "myTextureSampler");
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture);

	Objects bullets;
	Objects dodecas;

	static double start_time = glfwGetTime();
	static double lastTime = glfwGetTime();
	double last_dodeca_time = start_time;
	double current_time;
	float delta_time;

	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0) {
		for (int i = 0; i < bullets.size(); ++i) {
			for (int j = 0; j < dodecas.size(); ++j) {
				if (are_close(bullets[i].pseudo_center, dodecas[j].pseudo_center,
					bullets[i].pseudo_radius + dodecas[j].pseudo_radius)) {
					bullets[i].destroyed = dodecas[j].destroyed = true;
				}
			}
		}

		current_time = glfwGetTime();
		delta_time = float(current_time - lastTime);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
			Object bullet;
			loadOBJ("sphere.obj", bullet.vertices, bullet.uvs, bullet.normals);
			bullet.texture_id = 0;
			bullet.init();
			bullet.velocity = get_direction() / 10;
			bullet.move(get_camera_position() + get_direction());
			bullets.push_back(bullet);
		}

		
		current_time = glfwGetTime();
		delta_time = float(current_time - start_time);
		if (current_time - last_dodeca_time > 2) {
			last_dodeca_time = current_time;
			Object dodeca;
			loadOBJ("dode.obj", dodeca.vertices, dodeca.uvs, dodeca.normals);
			dodeca.texture_id = 0;
			dodeca.init();
			dodeca.move({ rand_float() / 2,rand_float() / 2,rand_float() / 2 });
			dodecas.push_back(dodeca);
		}

		update(bullets, delta_time / 100);
		update(dodecas, delta_time / 100);

		glfwSwapBuffers(window);
		glfwPollEvents();

		lastTime = current_time;
	}
}
