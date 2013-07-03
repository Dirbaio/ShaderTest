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

#include "record.h"
#include "dsp.h"

#include<cstdio>
#include<fstream>

using namespace std;
using namespace sf;

float dt;
float tim = 0;
Window* app;
float frameTime;
int frameCount;
int window_width = 800;
int window_height = 600;

#define texSize 2048
#define texRows 4
Texture tex;

float beat;

Shader* loadShader(string vertex, string fragment)
{
    cerr << "* Loading shader: " << vertex << " " << fragment << endl;
    Shader* t = new Shader();
    if (!t->loadFromFile(vertex, fragment))
        exit(1);

    return t;
}

unsigned char fftTex[texRows][texSize*4];

FFT fft;

void streamRecordCallback(short* buffer, int samples)
{
    float buf[samples];

    for(int i = 0; i < samples; i++)
        buf[i] = (float(buffer[2*i]) + float(buffer[2*i+1]))/float(1<<16);

    fft.forward(buf, samples);

    for(int i = 0; i < texSize; i++)
    {
        float v = fft.spectrum[i];
/*        v -= fft.spectrum[i/2]*0.1;
        v -= fft.spectrum[i/3]*0.05;
        v -= fft.spectrum[i/4]*0.02;*/
        v *= 3;
        if(v < 0) v = 0;
        if(v > 1) v = 1;
        fftTex[0][i*4] = (unsigned char)(v*255);
        fftTex[0][i*4] = (unsigned char)(v*2*255);
    }
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

GLuint genTexture() {
    // We create a single float channel 512^2 texture
    GLuint texHandle;
    glGenTextures(1, &texHandle);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texHandle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RED, GL_FLOAT, NULL);

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

GLuint renderHandle, computeHandle;

void updateTex(float time) {
    glUseProgram(computeHandle);
    glUniform1f(glGetUniformLocation(computeHandle, "time"), time);
    glDispatchCompute(512/16, 512/16, 1); // 512^2 threads in blocks of 16^2
    checkErrors("Dispatch compute shader");
}

int main(int argc, char** argv)
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
    Shader* shader = loadShader("vertex.glsl", "fragment.glsl");
    Shader* fftShader = loadShader("vertex.glsl", "fragment-fft.glsl");
    bool running = true;

    tex.create(texSize, texRows);
    tex.setSmooth(true);


//    sf::Thread thread(&startRecording, &streamRecordCallback);
//    thread.launch(); // start the thread (internally calls threadFunc(5))

    GLuint texHandle = genTexture();
    renderHandle = genRenderProg(texHandle);
    computeHandle = genComputeProg(texHandle);

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

//        tex.update(&fftTex[0][0]);

        if(frameTime >= 1)
        {
            cout<<"FPS "<<frameCount/frameTime<<endl;
            frameTime = 0;
            frameCount = 0;
        }

        beat -= dt*2;
        if(beat < 0) beat = 0;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the depth and colour buffers
        if(true)
        {
            updateTex(tim);
            glUseProgram(renderHandle);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
/*

            Shader::bind(shader);
            shader->setParameter("time", tim);
            shader->setParameter("beat", beat);

            glBegin(GL_QUADS);
            float aspectRatio = (float)app->getSize().x / (float)app->getSize().y;
            glTexCoord2f(-aspectRatio, -1); glVertex2f(-1, -1);
            glTexCoord2f(aspectRatio, -1); glVertex2f(1, -1);
            glTexCoord2f(aspectRatio, 1); glVertex2f(1, 1);
            glTexCoord2f(-aspectRatio, 1); glVertex2f(-1, 1);
            glEnd();*/
        }
/*
        Shader::bind(fftShader);
        fftShader->setParameter("fft", tex);

        glBegin(GL_QUADS);
        glTexCoord2f(-1, -1); glVertex2f(-1, -1);
        glTexCoord2f(1, -1); glVertex2f(1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, 1);
        glTexCoord2f(-1, 1); glVertex2f(-1, 1);
        glEnd();
*/
        app->display();
    }

//    thread.terminate(); //Terminate ALL the threads!
    return EXIT_SUCCESS;
}


