#define GL_GLEXT_PROTOTYPES 1

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

using namespace std;
using namespace sf;

float dt;
float tim = 0;
Window* app;
float frameTime;
int frameCount;
int window_width = 512;
int window_height = 512;

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


    //sf::Thread thread(&startRecording, &streamRecordCallback);
  //  thread.launch(); // start the thread (internally calls threadFunc(5))

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

        tex.update(&fftTex[0][0]);

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
            Shader::bind(shader);
            shader->setParameter("time", tim);
            shader->setParameter("beat", beat);

            glBegin(GL_QUADS);
            float aspectRatio = (float)app->getSize().x / (float)app->getSize().y;
            glTexCoord2f(-aspectRatio, -1); glVertex2f(-1, -1);
            glTexCoord2f(aspectRatio, -1); glVertex2f(1, -1);
            glTexCoord2f(aspectRatio, 1); glVertex2f(1, 1);
            glTexCoord2f(-aspectRatio, 1); glVertex2f(-1, 1);
            glEnd();
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

 //   thread.terminate(); //Terminate ALL the threads!
    return EXIT_SUCCESS;
}


