#define GL_GLEXT_PROTOTYPES 1

#include "GL/glew.h"

#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <GL/gl.h>
#include <GL/glu.h>
#include "GL/glext.h"
#include <iostream>
#include <cmath>
#include <pulse/simple.h>
#include <pulse/error.h>

#include<cstdio>
#include<fstream>

#include<cstring>
#include "Octree.h"

using namespace std;
using namespace sf;

float dt;
float tim = 0;
Window* app;
float frameTime;
int frameCount;
int window_width = 1024;
int window_height = 1024;



Shader* loadShader(string vertex, string fragment)
{
	cerr << "* Loading shader: " << vertex << " " << fragment << endl;
	Shader* t = new Shader();
	if (!t->loadFromFile(vertex, fragment))
		exit(1);

	return t;
}

void checkErrors(std::string desc) {
	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		fprintf(stderr, "OpenGL error in \"%s\": %s (%d)\n", desc.c_str(), gluErrorString(e), e);
		exit(20);
	}
}

GLuint genRenderProg(GLuint texHandle) {
	GLuint progHandle = glCreateProgram();
	GLuint vp = glCreateShader(GL_VERTEX_SHADER);
	GLuint fp = glCreateShader(GL_FRAGMENT_SHADER);

	const char *vpSrc[] = {
		"#version 430\n",
		"in vec2 pos;\
		out vec2 texCoord;\
		void main() {\
			texCoord = pos*0.5f + 0.5f;\
			gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);\
		}"
	};

	const char *fpSrc[] = {
		"#version 430\n",
		"uniform sampler2D srcTex;\
		in vec2 texCoord;\
		out vec4 color;\
		void main() {\
			color = texture(srcTex, texCoord);\
		}"
	};

	glShaderSource(vp, 2, vpSrc, NULL);
	glShaderSource(fp, 2, fpSrc, NULL);

	glCompileShader(vp);
	int rvalue;
	glGetShaderiv(vp, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		fprintf(stderr, "Error in compiling vp\n");
		exit(30);
	}
	glAttachShader(progHandle, vp);

	glCompileShader(fp);
	glGetShaderiv(fp, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		fprintf(stderr, "Error in compiling fp\n");
		exit(31);
	}
	glAttachShader(progHandle, fp);

	glBindFragDataLocation(progHandle, 0, "color");
	glLinkProgram(progHandle);

	glGetProgramiv(progHandle, GL_LINK_STATUS, &rvalue);
	if (!rvalue) {
		fprintf(stderr, "Error in linking sp\n");
		exit(32);
	}

	glUseProgram(progHandle);
	glUniform1i(glGetUniformLocation(progHandle, "srcTex"), 0);

	GLuint vertArray;
	glGenVertexArrays(1, &vertArray);
	glBindVertexArray(vertArray);

	GLuint posBuf;
	glGenBuffers(1, &posBuf);
	glBindBuffer(GL_ARRAY_BUFFER, posBuf);
	float data[] = {
		-1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*8, data, GL_STREAM_DRAW);
	GLint posPtr = glGetAttribLocation(progHandle, "pos");
	glVertexAttribPointer(posPtr, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(posPtr);

	checkErrors("Render shaders");
	return progHandle;
}

int texsize = 1024;

GLuint genTexture() {
	// We create a single float channel 512^2 texture
	GLuint texHandle;
	glGenTextures(1, &texHandle);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texHandle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_RED, GL_FLOAT, NULL);

	// Because we're also using this tex as an image (in order to write to it),
	// we bind it to an image unit as well
	glBindImageTexture(0, texHandle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
	checkErrors("Gen texture");
	return texHandle;
}

bool getFileContents(const std::string& filePath, std::vector<char>& buffer, int &fileLength) {
	std::ifstream file(filePath.c_str(), std::ios_base::binary);
	if (!file)
		return false;
	file.seekg(0, std::ios_base::end);
	fileLength = file.tellg();
	if (fileLength > 0) {
		file.seekg(0, std::ios_base::beg);
		buffer.resize(static_cast<std::size_t>(fileLength));
		file.read(&buffer[0], fileLength);
	}
	buffer.push_back('\0');
	return true;
}

GLuint genComputeProg(GLuint texHandle) {
	// Creating the compute shader, and the program object containing the shader
	GLuint progHandle = glCreateProgram();
	GLuint cs = glCreateShader(GL_COMPUTE_SHADER);

	// In order to write to a texture, we have to introduce it as image2D.
	// local_size_x/y/z layout variables define the work group size.
	// gl_GlobalInvocationID is a uvec3 variable giving the global ID of the thread,
	// gl_LocalInvocationID is the local index within the work group, and
	// gl_WorkGroupID is the work group's index

	std::vector<char> fileContents;
	int length;
	getFileContents("compute.comp",fileContents,length);
	GLchar* a[1];
	a[0] = &(fileContents[0]);
	glShaderSource(cs, 1, (const GLchar**)a, &length);

	glCompileShader(cs);
	int rvalue;
	glGetShaderiv(cs, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		fprintf(stderr, "Error in compiling the compute shader\n");
		GLchar log[10240];
		GLsizei length;
		glGetShaderInfoLog(cs, 10239, &length, log);
		fprintf(stderr, "Compiler log:\n%s\n", log);
		exit(40);
	}
	glAttachShader(progHandle, cs);

	glLinkProgram(progHandle);
	glGetProgramiv(progHandle, GL_LINK_STATUS, &rvalue);
	if (!rvalue) {
		fprintf(stderr, "Error in linking compute shader program\n");
		GLchar log[10240];
		GLsizei length;
		glGetProgramInfoLog(progHandle, 10239, &length, log);
		fprintf(stderr, "Linker log:\n%s\n", log);
		exit(41);
	}
	glUseProgram(progHandle);

	glUniform1i(glGetUniformLocation(progHandle, "destTex"), 0);

	checkErrors("Compute shader");
	return progHandle;
}


struct block
{
		float r, g, b, a;
};

int worldsize = 32;
Octree o;

GLuint genSsbo()
{
	GLuint ssbo;
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

	vector<SsboOctree> v;
	o.toSsbo(v);

	int size = v.size()*sizeof(SsboOctree);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_STATIC_DRAW);

	void* data = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, size, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
	memcpy(data, &v[0], size);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	return ssbo;
}

GLuint renderHandle, computeHandle, ssbo;

int asdf(int x)
{
	x = (x%6 + 6)%6;
	if(x > 2) x = 5-x;
	return x;
}
int main()
{
	// Create the main window
	app = new Window(sf::VideoMode(window_width, window_height, 32), "SFML OpenGL");
	//app->setVerticalSyncEnabled(true);

	// Create a clock for measuring time elapsed
	sf::Clock clock;

	// Set color and depth clear value
	glClearDepth(1.0f);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bool running = true;

	//NO BORRAR GRACIAS
	Texture tex;
	tex.create(1, 1);
	tex.setSmooth(true);

	GLuint texHandle = genTexture();
	renderHandle = genRenderProg(texHandle);
	computeHandle = genComputeProg(texHandle);
	ssbo = genSsbo();

	int num = 500;

	for(int x = -num; x <= num; x++)
			for(int z = -num; z < num; z++)
				o.set(x, asdf(x)+asdf(z), z, OCTREE_SIZE, 4);

	float ang = 0;
	float zoom = 30;
	// Start game loop
	while (app->isOpen())
	{
		// Process events
		sf::Event event;
		while (app->pollEvent(event))
		{
			// Close window : exit
			if (event.type == sf::Event::Closed)
				app->close();

			// Escape key : exit
			if ((event.type == sf::Event::KeyPressed) && (event.key.code == Keyboard::Escape))
				app->close();

			if ((event.type == sf::Event::KeyPressed) && (event.key.code == Keyboard::Space))
				running = !running;

			// Resize event : adjust viewport
			if (event.type == sf::Event::Resized)
			{
				window_width = event.size.width;
				window_height = event.size.height;
				glViewport(0, 0, window_width, window_height);
			}
		}

		app->setActive();

		dt = clock.getElapsedTime().asSeconds();

		clock.restart();
		if(running)
			tim += dt;

		frameTime += dt;
		frameCount++;

		if(frameTime >= 1) {
			cout<<"FPS "<<frameCount/frameTime<<endl;
			frameTime = 0;
			frameCount = 0;
		}

		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
			ang -= dt;
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
			ang += dt;
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::A))
			zoom -= dt*3;
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Z))
			zoom += dt*3;

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the depth and colour buffers

		glUseProgram(computeHandle);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
		glUniform1f(glGetUniformLocation(computeHandle, "time"), tim);
		float cx = 16+zoom*sin(ang);
		float cy = 16+zoom;
		float cz = 16+zoom*cos(ang);
		float dx = -cx+16;
		float dy = -cy+16;
		float dz = -cz+16;
		glUniform3f(glGetUniformLocation(computeHandle, "eyePos"), cx, cy, cz);
		glUniform3f(glGetUniformLocation(computeHandle, "camForward"), dx, dy, dz);
		glUniform3f(glGetUniformLocation(computeHandle, "camUp"), 0, 1, 0);
		glDispatchCompute(texsize/16, texsize/16, 1);
		checkErrors("Dispatch compute shader");

		glUseProgram(renderHandle);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		app->display();
	}

	//    thread.terminate(); //Terminate ALL the threads!
	return EXIT_SUCCESS;
}


