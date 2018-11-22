#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Log {
	template<typename... Ts> void error(Ts&&... xs) { std::cout << "\033[1;31merror:\033[0m "; (std::cout << ... << xs); }
	template<typename... Ts> void info(Ts&&... xs) { std::cout << "info: "; (std::cout << ... << xs); }
} lg;
struct Shader {
    unsigned id;
	explicit Shader(GLenum type):id(glCreateShader(type)){}
    void source(GLsizei count, const GLchar* const* text, const GLint* length) { glShaderSource(id, count, text, length); }
    void source(GLsizei count, const std::string& text, const GLint* length) { auto text2 = text.c_str(); source(count, &text2, length); }
    void compile() {
        glCompileShader(id);
        int success;
        char infoLog[512];
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (not success) {
            glGetShaderInfoLog(id, 512, nullptr, infoLog);
            lg.error("shader compilation failed: ", infoLog);
            std::exit(1);
        }
    }
    void free() { glDeleteShader(id); }
};
struct Uniform {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-c-copy-assignment-signature"
    void operator=(float x) { glUniform1f(id, x); }
    void operator=(int x) { glUniform1i(id, x); }
    void operator=(const glm::mat4& x) { glUniformMatrix4fv(id, 1, GL_FALSE, glm::value_ptr(x)); }
    void operator=(const glm::vec3& x) { glUniform3f(id, x.x, x.y, x.z); }
#pragma clang diagnostic pop
	int id;
};
struct Program {
    unsigned id;
    Program():id(glCreateProgram()){}
    void attach(const Shader& shader) { glAttachShader(id, shader.id); }
    void link() {
        glLinkProgram(id);
        int success;
        char infoLog[512];
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if (not success) {
            glGetProgramInfoLog(id, 512, nullptr, infoLog);
            lg.error("shader linking failed: ", infoLog);
            std::exit(1);
        }
    }
    Uniform locateUniform(const char* name) {
        return Uniform{glGetUniformLocation(id, name)};
    }
    void use() { glUseProgram(id); }
};
struct Window {
    Window(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share):ptr(glfwCreateWindow(width, height, title, monitor, share)) {
        if (ptr == nullptr) {
            lg.error("failed to create GLFW window\n");
            glfwTerminate();
            exit(1);
        }
    }
    void makeContextCurrent() { glfwMakeContextCurrent(ptr); }
    int getKey(int key) { return glfwGetKey(ptr, key); }
    float xkey(int key) { return (float)getKey(key); }
    float xkeyjoy(int keypos, int keyneg) { return xkey(keypos) - xkey(keyneg); }
    int shouldClose() { return glfwWindowShouldClose(ptr); }
    void setShouldClose(int value) { glfwSetWindowShouldClose(ptr, value); }
    void setInputMode(int mode, int value) { glfwSetInputMode(ptr, mode, value); }
    void swapBuffers() { glfwSwapBuffers(ptr); }
    void onFramebufferSize(GLFWwindowsizefun callback) { glfwSetWindowSizeCallback(ptr, callback); }
    void onCursorPos(GLFWcursorposfun callback) { glfwSetCursorPosCallback(ptr, callback); }
    GLFWwindow* ptr;
};
struct VAO {
    VAO():id(0){ glGenVertexArrays(1, &id); }
    void bind() { glBindVertexArray(id); }
    unsigned id;
};
struct VBO {
    VBO():id(0) { glGenBuffers(1, &id); }
    void bindBuffer(GLenum target) { glBindBuffer(target, id); }
    template <unsigned n> void xbindAndBufferStatic(const float(&vertices)[n]) {
        xbindAndBufferStatic(vertices, n);
    }
    void xbindAndBufferStatic(const std::vector<float>& vertices) {
    	xbindAndBufferStatic(vertices.data(), (unsigned)vertices.size());
    }
    void xbindAndBufferStatic(const float* vertices, unsigned n) {
		bindBuffer(GL_ARRAY_BUFFER);
		glBufferData(GL_ARRAY_BUFFER, n * sizeof(float), vertices, GL_STATIC_DRAW);
	}
    unsigned id;
};
struct EBO {
    EBO():id(0) { glGenBuffers(1, &id); }
    void bindBuffer(GLenum target) { glBindBuffer(target, id); }
    template <unsigned n> void xbindAndBufferStatic(unsigned(&indices)[n]) {
    	xbindAndBufferStatic(indices, n);
	}
	void xbindAndBufferStatic(const std::vector<unsigned>& indices) {
    	xbindAndBufferStatic(indices.data(), (unsigned)indices.size());
	}
	void xbindAndBufferStatic(const unsigned* indices, unsigned n) {
        bindBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, n * sizeof(unsigned), indices, GL_STATIC_DRAW);
    }
    unsigned id;
};
struct GLFW {
	GLFW() { glfwInit(); }
	void windowHint(int hint, int value) { glfwWindowHint(hint, value); }
	void pollEvents() { glfwPollEvents(); }
	void terminate() { glfwTerminate(); }
	double time() { return glfwGetTime(); }
	void xhintContextVersion(int major, int minor) {
		windowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
		windowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	}
};
struct Image {
	int width, height, channelCount;
	unsigned char* data;
	static Image load(const std::string& path) {
		Image image;
		stbi_set_flip_vertically_on_load(true);
		image.data = stbi_load(path.c_str(), &image.width, &image.height, &image.channelCount, 0);
		if (not image.data) {
			lg.error("failed to load image ", path);
			std::exit(1);
		}
		return image;
	}
	void free() { stbi_image_free(data); }
private:
	Image():width(0),height(0),channelCount(0),data(nullptr){}
};
struct Texture {
	unsigned id;
	Texture():id(0){ glGenTextures(1, &id); }
	void bind(GLenum target) { glBindTexture(target, id); }
	void xactivateAndBind(GLenum slot, GLenum target) {
		glActiveTexture(slot);
		glBindTexture(target, id);
	}
};
struct ScreenInfo {
	int width, height;
};

template <typename T, unsigned n> constexpr unsigned arraySize(T(&)[n]) { return n; }
std::string readFile(const std::string& path) {
    std::ostringstream oss;
    std::ifstream file(path);
    file >> oss.rdbuf();
    return oss.str();
}

Shader shaderFromFile(const std::string& path, GLenum type) {
	auto src = readFile(path);
	auto shader = Shader(type);
	shader.source(1, src, nullptr);
	shader.compile();
	return shader;
}
Program shaderProgramFromShaders(const Shader& vertex, const Shader& fragment) {
	auto program = Program();
	program.attach(vertex);
	program.attach(fragment);
	program.link();
	return program;
}
Program shaderProgramFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
	auto vertex = shaderFromFile(vertexPath, GL_VERTEX_SHADER);
	auto fragment = shaderFromFile(fragmentPath, GL_FRAGMENT_SHADER);
	auto program = shaderProgramFromShaders(vertex, fragment);
	vertex.free();
	fragment.free();
	return program;
}
Texture textureFromFile(const std::string& path, GLenum format) {
	auto img = Image::load(path);
	auto tex = Texture();
	tex.bind(GL_TEXTURE_2D);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.data);
	img.free();
	glGenerateMipmap(GL_TEXTURE_2D);
	return tex;
}
void loadGLAD() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		lg.error("failed to initialize GLAD\n");
		std::exit(-1);
	}
}
glm::vec3 rgb(int r, int g, int b) { return glm::vec3((float)r / 255, (float)g / 255, (float)b / 255); }
float clamp(float a, float b, float x) { return std::min(std::max(x, a), b); }
float sgn(float x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
float mix(float a, float b, float x) { return x * a + (1 - x) * b; }

std::minstd_rand rng((unsigned long)std::chrono::high_resolution_clock::now().time_since_epoch().count()); // NOLINT(cert-err58-cpp)
template <typename T> T uniform(T a, T b) { return std::uniform_real_distribution<T>(a, b)(rng); }

struct CameraFPS {
	glm::vec3 pos, up, front;
	float yaw, pitch;
	glm::vec2 last;
	bool init;
	explicit CameraFPS(glm::vec3 pos):pos(pos),up(0.0f, 1.0f, 0.0f),front(1.0f, 0.0f, 0.0f),yaw(0.0f),pitch(0.0f),last(0.0f, 0.0f),init(false){}
	void handleCursorPos(float x, float y) {
		if (not init) {
			init = true;
			last = {x, y};
		}
		auto dx = x - last.x;
		auto dy = last.y - y;
		auto sensitivity = 0.1f;
		last = {x, y};
		yaw += dx * sensitivity;
		pitch += dy * sensitivity;
		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;
		front = glm::normalize(glm::vec3(
			cosf(glm::radians(pitch)) * cosf(glm::radians(yaw)),
			sinf(glm::radians(pitch)),
			cosf(glm::radians(pitch)) * sinf(glm::radians(yaw))
		));
	}
	void handleKeyboard(float x, float y, float z, float deltaTime) {
		auto cameraSpeed = 20.0f;
		pos += cameraSpeed * deltaTime * x * right();
		pos += cameraSpeed * deltaTime * y * up;
		pos += cameraSpeed * deltaTime * z * -front;
	}
	glm::vec3 right() { return glm::cross(front, up); }
	glm::mat4 viewMatrix() {
		return glm::lookAt(pos, pos + front, up);
	}
};
struct Water {
	VAO vao;
	VBO vbo;
	EBO ebo;
	Program shader;
	Uniform utime, upv, umodel, uviewpos;
	std::vector<float> vertices;
	std::vector<unsigned> indices;
	Water(unsigned width, unsigned depth, const std::string& vertPath, const std::string& fragPath, glm::vec3 sunPos):
		vao(),
		vbo(),
		ebo(),
		shader(shaderProgramFromFiles(vertPath, fragPath)),
		utime(shader.locateUniform("time")),
		upv(shader.locateUniform("trans_pv")),
		umodel(shader.locateUniform("trans_model")),
		uviewpos(shader.locateUniform("view_pos")),
		vertices(),
		indices(2*3*width*depth)
	{
		for (auto x=0; x<width+1; ++x) {
			for (auto z=0; z<depth+1; ++z) {
				vertices.push_back((float)x);
				vertices.push_back(0);
				vertices.push_back((float)z);
				if (x < width and z < depth) {
					indices[3*(x*depth+z)] = ((unsigned)((width+1)*x+z));
					indices[3*(x*depth+z)+1] = ((unsigned)((width+1)*x+z+1));
					indices[3*(x*depth+z)+2] = ((unsigned)((width+1)*(x+1)+z));
					indices[3*(x*depth+z)+3*width*depth] = ((unsigned)((width+1)*x+z+1));
					indices[3*(x*depth+z)+3*width*depth+1] = ((unsigned)((width+1)*(x+1)+z));
					indices[3*(x*depth+z)+3*width*depth+2] = ((unsigned)((width+1)*(x+1)+z+1));
				}
			}
		}

		vao.bind();
		vbo.xbindAndBufferStatic(vertices);
		ebo.xbindAndBufferStatic(indices);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(0 * sizeof(float)));
		glEnableVertexAttribArray(0);

		shader.use();
		umodel = glm::mat4(1.0f);
		shader.locateUniform("sun.pos") = sunPos;
		shader.locateUniform("sun.color") = glm::vec3(1.0f);
	}
	void draw(float time, const glm::mat4& transPV, glm::vec3 viewPos, bool transparent) {
		vao.bind();
		shader.use();
		utime = time;
		upv = transPV;
		uviewpos = viewPos;
		glDrawElements(GL_TRIANGLES, (unsigned)indices.size() / (transparent ? 2 : 1), GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
	}
};
struct ToggleButton {
	bool state;
	bool pressed;
	explicit ToggleButton(bool initialState):state(initialState),pressed(false){}
	bool update(int keyresult) {
		if (pressed and keyresult != GLFW_PRESS)
			pressed = false;
		else if (not pressed and keyresult == GLFW_PRESS) {
			pressed = true, state = not state;
			return true;
		}
		return false;
	}
	bool operator*() { return state; }
};
const glm::vec3 gravity = {0.0f, -9.80665, 0.0f}; // NOLINT(cert-err58-cpp)
const float waterDensity = 998.23; // kg/m^3
const float airDensity = 1.225; // kg/m^3
const float woodDensity = 600.0f;
const float plateDragCoefficient = 1.17;

struct CubeVertices {
	VBO vbo;
	VAO vao;
	CubeVertices():vbo(),vao(){
		vao.bind();
		vbo.xbindAndBufferStatic(rawData);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(0 * sizeof(float)));
		glEnableVertexAttribArray(0);
	}
	void draw() {
		vao.bind();
		glDrawArrays(GL_TRIANGLES, 0, arraySize(rawData));
		glBindVertexArray(0);
	}
	static constexpr float rawData[] = {
		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
		-0.5f, -0.5f, 0.5f,

		-0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,

		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,

		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		-0.5f, -0.5f, 0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, -0.5f,
	};
};

struct Raft {
	CubeVertices& cubev;
	Program shader;
	Uniform upv, umodel;
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 scale;
	float rotation;
	float angularVelocity;
	float mass;
	Raft(glm::vec3 position, float mass, glm::vec3 scale, const std::string& vertPath, const std::string& fragPath, CubeVertices& cubev):
		cubev(cubev),
		shader(shaderProgramFromFiles(vertPath, fragPath)),
		upv(shader.locateUniform("trans_pv")),
		umodel(shader.locateUniform("trans_model")),
		position(position),
		velocity(),
		scale(scale),
		rotation(0.0f),
		angularVelocity(0.0f),
		mass(mass)
	{
	}

	static float waveHeightAtPoint(glm::vec3 vertex_pos, float time) {
		float wavePresence = (sinf((vertex_pos.x + vertex_pos.z + time)/16) + 1) / 2;
		float x = vertex_pos.z / 8 + vertex_pos.x / 32 + time / 2;
		float height = 4 * wavePresence * (sinf(x) + sinf(2*x) + sinf(3*x));
//		float height = 0;
		return height;
	}
	static glm::vec3 forceAtPoint(glm::vec3 position, float velocity, glm::vec3 scale, float mass, float time) {
		auto waveHeight = waveHeightAtPoint(position, time);
		auto lowerEdgeY = position.y - scale.y / 2;
		auto submergedHeight = clamp(0.0f, scale.y, waveHeight - lowerEdgeY);
		auto displacedWaterVolume = submergedHeight * scale.x * scale.z;
		auto averageFluidDensity = mix(waterDensity, airDensity, submergedHeight / scale.y);		
		auto area = scale.x * scale.z;
		auto up = glm::vec3(0.0f, 1.0f, 0.0f);

		auto weight = mass * gravity;
		auto buoyancy = - waterDensity * displacedWaterVolume * gravity;
		auto drag = -sgn(velocity) * up * 0.5f * averageFluidDensity * powf(velocity, 2) * plateDragCoefficient * area;

		auto total = weight + buoyancy + drag;
		return total;
	}
	static float dragAtPoint(glm::vec3 position, float velocity, glm::vec3 scale, float mass, float time) {
		auto waveHeight = waveHeightAtPoint(position, time);
		auto fluidDensity = position.y > waveHeight ? airDensity : waterDensity;
		auto area = scale.x * scale.z;
		auto drag = -sgn(velocity) * 0.5f * fluidDensity * powf(velocity, 2) * plateDragCoefficient * area;
		return drag;
	}
	float computeAngularAcceleration(float time) {
		auto halfScale = scale;
		halfScale.z /= 2;
		auto leftPos = position;
		leftPos.z -= scale.z / 4 * cosf(rotation);
		leftPos.y -= scale.z / 4 * sinf(rotation);
		auto rightPos = position;
		rightPos.z += scale.z / 4 * cosf(rotation);
		rightPos.y += scale.z / 4 * sinf(rotation);

		auto leftForce = forceAtPoint(leftPos, velocity.y, halfScale, mass/2, time).y;
		auto rightForce = forceAtPoint(rightPos, velocity.y, halfScale, mass/2, time).y;
		auto rightAngDrag = dragAtPoint(rightPos, angularVelocity * scale.z / 4, halfScale, mass/2, time);
		auto leftAngDrag = dragAtPoint(leftPos, angularVelocity * scale.z / 4, halfScale, mass/2, time);

		auto rightTorque = (rightForce * cosf(rotation) + rightAngDrag) * (scale.z / 4);
		auto leftTorque = (leftForce * cosf(-rotation) - leftAngDrag) * (scale.z / 4);
		auto torque = rightTorque - leftTorque;

		auto momentOfInertia = (1.0f / 12) * mass * (powf(scale.z, 2) + powf(scale.y, 2));
		auto angularAcceleration = torque / momentOfInertia;

		return angularAcceleration;
	}
	void update(float deltaTime, float time) {
		auto force = forceAtPoint(position, velocity.y, scale, mass, time);
		auto angularAcceleration = computeAngularAcceleration(time);
		velocity += deltaTime * force / mass;
		position += deltaTime * velocity;
		angularVelocity += deltaTime * angularAcceleration;
		rotation += deltaTime * angularVelocity;
	}
	void draw(const glm::mat4& transPV) {
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = glm::rotate(model, -rotation, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, scale);
		shader.use();
		upv = transPV;
		umodel = model;
		cubev.draw();
	}
};
struct Sun {
	CubeVertices& cubev;
	Program shader;
	Uniform upv, umodel;
	glm::vec3 position;
	glm::vec3 scale;
	Sun(glm::vec3 position, glm::vec3 scale, const std::string& vertPath, const std::string& fragPath, CubeVertices& cubev):
		cubev(cubev),
		shader(shaderProgramFromFiles(vertPath, fragPath)),
		upv(shader.locateUniform("trans_pv")),
		umodel(shader.locateUniform("trans_model")),
		position(position),
		scale(scale)
	{}
	void draw(const glm::mat4& transPV) {
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = glm::scale(model, scale);
		shader.use();
		upv = transPV;
		umodel = model;
		cubev.draw();
	}
};

auto screen = ScreenInfo { 1920, 1080 };
auto camera = CameraFPS({470.0f, 10.0f, 500.0f}); // NOLINT(cert-err58-cpp)

int main() {
	auto glfw = GLFW();
	glfw.xhintContextVersion(3, 3);
	glfw.windowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    auto window = Window(screen.width, screen.height, "Surfaces", nullptr, nullptr);
    window.makeContextCurrent();
    auto icon = Image::load("icon2.png");
    GLFWimage icons[1] = {GLFWimage{icon.width, icon.height, icon.data}};
    glfwSetWindowIcon(window.ptr, 1, icons);
    loadGLAD();
    glViewport(0, 0, screen.width, screen.height);
    glEnable(GL_DEPTH_TEST);
	window.onFramebufferSize([](GLFWwindow *, int width, int height) {
		lg.info("resizing to ", width, " ", height, "\n");
		glViewport(0, 0, width, height);
		screen.width = width;
		screen.height = height;
	});
	window.setInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	window.onCursorPos([](GLFWwindow*, double xpos, double ypos){ camera.handleCursorPos((float)xpos, (float)ypos); });

	auto cubeVertices = CubeVertices();
	auto sun = Sun({550.0f, 30.0f, 550.0f}, {10.0f, 10.0f, 10.0f}, "standard.vert", "sun.frag", cubeVertices);
	auto water = Water(1000, 1000, "water.vert", "water.frag", sun.position);
	auto raftScale = glm::vec3(10.0f, 0.5f, 10.0f);
	auto raft = Raft({500.0f, 6.0f, 500.0f}, raftScale.x * raftScale.y * raftScale.z * woodDensity, raftScale, "standard.vert", "raft.frag", cubeVertices);

	auto backgroundColor = rgb(0x00, 0x2b, 0x36);
	auto lastTime = (float)glfw.time();
	auto cameraTime = lastTime;
	auto physicsTime = lastTime;
	auto paused = ToggleButton(false);
	auto transparent = ToggleButton(false);

	while (not window.shouldClose()) {

        // handle input

		auto cameraDeltaTime = [&](){
			auto time = (float)glfw.time();
			auto deltaTime = time - lastTime;
			lastTime = time;
			return deltaTime;
		}();
		auto physicsDeltaTime = *paused ? 0.0f : cameraDeltaTime;
		cameraTime += cameraDeltaTime;
		physicsTime += physicsDeltaTime;

		if (window.getKey(GLFW_KEY_ESCAPE) == GLFW_PRESS)
            window.setShouldClose(true);
		paused.update(window.getKey(GLFW_KEY_SPACE));
		transparent.update(window.getKey(GLFW_KEY_F3));

		camera.handleKeyboard(window.xkeyjoy(GLFW_KEY_D, GLFW_KEY_A), window.xkeyjoy(GLFW_KEY_E, GLFW_KEY_Q), window.xkeyjoy(GLFW_KEY_S, GLFW_KEY_W), cameraDeltaTime);

		auto view = camera.viewMatrix();
		auto projection = glm::perspective(glm::radians(45.0f), (float)screen.width / screen.height, 0.1f, 3000.0f);
		auto transPV = projection * view;

		// render

        glClearColor(backgroundColor.x, backgroundColor.y, backgroundColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        water.draw(physicsTime, transPV, camera.pos, *transparent);
        raft.update(physicsDeltaTime, physicsTime);
        raft.draw(transPV);
        sun.draw(transPV);

        // swap buffers

        window.swapBuffers();
        glfw.pollEvents();

    }

    glfw.terminate();
    return 0;

}
