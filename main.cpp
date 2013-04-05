#define GL_GLEXT_PROTOTYPES 1

#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <GL/gl.h>
#include <GL/glu.h>
#include "GL/glext.h"
#include <iostream>

using namespace std;
using namespace sf;

float dt;
float tim = 0;
Window* theApp;
float frameTime;
int frameCount;
int window_width = 800;
int window_height = 600;


Shader* loadShader(string vertex, string fragment)
{
    cerr << "* Loading shader: " << vertex << " " << fragment << endl;
    Shader* t = new Shader();
    if (!t->loadFromFile(vertex, fragment))
        exit(1);

    return t;
}

int main(int argc, char** argv)
{
    // Create the main window
    Window app (sf::VideoMode(window_width, window_height, 32), "SFML OpenGL");
    theApp = &app;

    //	app.setVerticalSyncEnabled(true);

    // Create a clock for measuring time elapsed
    sf::Clock clock;

    // Set color and depth clear value
    glClearDepth(1.0f);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glDisable(GL_CULL_FACE);

    Shader* shader = loadShader("vertex.glsl", "fragment.glsl");

    // Start game loop
    while (app.isOpen())
    {
        // Process events
        sf::Event event;
        while (app.pollEvent(event))
        {
            // Close window : exit
            if (event.type == sf::Event::Closed)
            {
                app.close();
                return 0;
            }

            // Escape key : exit
            if ((event.type == sf::Event::KeyPressed) && (event.key.code == Keyboard::Escape))
            {
                app.close();
                return 0;
            }

            // Resize event : adjust viewport
            if (event.type == sf::Event::Resized)
            {
                window_width = event.size.width;
                window_height = event.size.height;
                glViewport(0, 0, window_width, window_height);
            }
        }

        app.setActive();

        dt = clock.getElapsedTime().asSeconds();
        clock.restart();
        tim += dt;

        frameTime += dt;
        frameCount++;

        if(frameTime >= 1)
        {
            cout<<"FPS "<<frameCount/frameTime<<endl;
            frameTime = 0;
            frameCount = 0;
        }


        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the depth and colour buffers
        shader->bind();
        shader->setParameter("time", tim);
        glBegin(GL_QUADS);
        glVertex2f(-1, -1);
        glVertex2f(1, -1);
        glVertex2f(1, 1);
        glVertex2f(-1, 1);
        glEnd();

        app.display();
    }

    return EXIT_SUCCESS;
}




