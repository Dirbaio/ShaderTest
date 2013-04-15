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


using namespace std;
using namespace sf;

#define fftSize 8192
#define texSize 4096
#define sampleRate 44100.0

// FFT from dsp.js, see below
class FFT
{
public:
    float spectrum[fftSize/2];
    float samples[fftSize];
    float real[fftSize];
    float imag[fftSize];
    int reverseTable[fftSize];
    float sinTable[fftSize];
    float cosTable[fftSize];

    FFT();
    void forward(float* buffer);
    void forward(float* buffer, int bufferSize);
};

FFT::FFT()
{
    int limit = 1,
            bit = fftSize >> 1;
    this->reverseTable[0] = 0; // ?!?!?
    while ( limit < fftSize ) {
        for ( int i = 0; i < limit; i++ ) {
            this->reverseTable[i + limit] = this->reverseTable[i] + bit;
        }

        limit = limit << 1;
        bit = bit >> 1;
    }

    for ( int i = 0; i < fftSize; i++ ) {
        this->sinTable[i] = sin(-M_PI/i);
        this->cosTable[i] = cos(-M_PI/i);
    }
    cout<<"FFT inited"<<endl;
}

void FFT::forward(float* buffer, int bufferSize)
{
    for(int i = 0; i < fftSize - bufferSize; i++)
        samples[i] = samples[i+bufferSize];

    int x = min(fftSize, bufferSize);
    for(int i = 0; i < x; i++)
        samples[fftSize-x+i] = buffer[bufferSize-x+i];

    forward(buffer);
}

void FFT::forward(float* buffer) {

    /*	if ( fftSize != buffer.length ) {
        cerr << "Supplied buffer is not the same size as defined FFT. FFT Size: " << fftSize <<
                " Buffer Size: " << buffer.length << endl;
    }*/

    for (int i = 0; i < fftSize; i++ ) {
        real[i] = buffer[reverseTable[i]];
        imag[i] = 0;
    }

    int halfSize = 1;
    int i = 0, off = 0;

    float phaseShiftStepReal,
            phaseShiftStepImag,
            currentPhaseShiftReal,
            currentPhaseShiftImag,
            tr,
            ti,
            tmpReal;

    while ( halfSize < fftSize ) {
        phaseShiftStepReal = cosTable[halfSize];
        phaseShiftStepImag = sinTable[halfSize];
        currentPhaseShiftReal = 1.0;
        currentPhaseShiftImag = 0.0;

        for ( int fftStep = 0; fftStep < halfSize; fftStep++ ) {
            i = fftStep;

            while ( i < fftSize ) {
                off = i + halfSize;
                tr = (currentPhaseShiftReal * real[off]) - (currentPhaseShiftImag * imag[off]);
                ti = (currentPhaseShiftReal * imag[off]) + (currentPhaseShiftImag * real[off]);

                real[off] = real[i] - tr;
                imag[off] = imag[i] - ti;
                real[i] += tr;
                imag[i] += ti;

                i += halfSize << 1;
            }

            tmpReal = currentPhaseShiftReal;
            currentPhaseShiftReal = (tmpReal * phaseShiftStepReal) - (currentPhaseShiftImag * phaseShiftStepImag);
            currentPhaseShiftImag = (tmpReal * phaseShiftStepImag) + (currentPhaseShiftImag * phaseShiftStepReal);
        }

        halfSize = halfSize << 1;
    }

    i = fftSize/2;
    while(i--) {
        spectrum[i] = 2 * sqrt(real[i] * real[i] + imag[i] * imag[i]) / fftSize;
    }
}



float dt;
float tim = 0;
Window* theApp;
float frameTime;
int frameCount;
int window_width = 800;
int window_height = 600;
float beat;

const float LN2 = 0.69314718055994530942;
const float rate = 44100;

enum FilterType {
    LPF = 0, /* low pass filter */
    HPF = 1, /* High pass filter */
    BPF = 2, /* band pass filter */
    NOTCH = 3, /* Notch Filter */
    PEQ = 4, /* Peaking band EQ filter */
    LSH = 5, /* Low shelf filter */
    HSH = 6 /* High shelf filter */
};

class Filter {
    float x1, x2, y1, y2;
    float b0, b1, b2, a0, a1, a2, a3, a4;

public:
    Filter()
    {
        /* zero initial samples */
        this->x1 = this->x2 = 0;
        this->y1 = this->y2 = 0;
    }

    Filter(FilterType type, float freq, float bandwidth, float dbGain = 0)
    {
        /* zero initial samples */
        this->x1 = this->x2 = 0;
        this->y1 = this->y2 = 0;
        set(type, freq, bandwidth, dbGain);
    }

    void set(FilterType type, float freq, float bandwidth, float dbGain = 0)
    {

        float A, omega, sn, cs, alpha, beta;
        float a0, a1, a2, b0, b1, b2;

        /* setup variables */
        A = pow(10, dbGain / 40);
        omega = 2 * M_PI * freq / rate;
        sn = sin(omega);
        cs = cos(omega);
        alpha = sn * sinh(LN2 / 2 * bandwidth * omega / sn);
        beta = sqrt(A + A);

        switch (type) {
        case LPF:
            b0 = (1 - cs) / 2;
            b1 = 1 - cs;
            b2 = (1 - cs) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        case HPF:
            b0 = (1 + cs) / 2;
            b1 = -(1 + cs);
            b2 = (1 + cs) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        case BPF:
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        case NOTCH:
            b0 = 1;
            b1 = -2 * cs;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        case PEQ:
            b0 = 1 + (alpha * A);
            b1 = -2 * cs;
            b2 = 1 - (alpha * A);
            a0 = 1 + (alpha /A);
            a1 = -2 * cs;
            a2 = 1 - (alpha /A);
            break;
        case LSH:
            b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
            b1 = 2 * A * ((A - 1) - (A + 1) * cs);
            b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
            a0 = (A + 1) + (A - 1) * cs + beta * sn;
            a1 = -2 * ((A - 1) + (A + 1) * cs);
            a2 = (A + 1) + (A - 1) * cs - beta * sn;
            break;
        case HSH:
            b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
            b1 = -2 * A * ((A - 1) + (A + 1) * cs);
            b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
            a0 = (A + 1) - (A - 1) * cs + beta * sn;
            a1 = 2 * ((A - 1) - (A + 1) * cs);
            a2 = (A + 1) - (A - 1) * cs - beta * sn;
            break;
        }

        /* precompute the coefficients */
        this->a0 = b0 / a0;
        this->a1 = b1 / a0;
        this->a2 = b2 / a0;
        this->a3 = a1 / a0;
        this->a4 = a2 / a0;

    }


    float sample(float inp) {
        float result = this->a0 * inp + this->a1 * this->x1 + this->a2 * this->x2 - this->a3 * this->y1 - this->a4 * this->y2;

        /* shift x1 to x2, sample to x1 */
        this->x2 = this->x1;
        this->x1 = inp;

        /* shift y1 to y2, result to y1 */
        this->y2 = this->y1;
        this->y1 = result;

        return result;

    }
};

Filter beatFlt (LPF, 100, 0.2);

class MySoundRecorder : public sf::SoundRecorder
{
    virtual bool onStart()
    {

        return true;
    }

    virtual bool onProcessSamples(const sf::Int16* samples, std::size_t count)
    {
        cout<<count<<" "<<count/rate<<endl;
        for(int i = 0; i < count; i++)
        {
            float s = samples[i] / float(1<<15);
            s = beatFlt.sample(s);
            s -= 0.9;
            beat = max(beat, s);
        }

        return true;
    }

    virtual void onStop()
    {
    }
};

Shader* loadShader(string vertex, string fragment)
{
    cerr << "* Loading shader: " << vertex << " " << fragment << endl;
    Shader* t = new Shader();
    if (!t->loadFromFile(vertex, fragment))
        exit(1);

    return t;
}

#define BUFSIZE 1024
#define MAXFRAME 44100
void renderQuad();
Texture tex;

int main(int argc, char** argv)
{
    // Create the main window
    Window app (sf::VideoMode(window_width, window_height, 32), "SFML OpenGL");
    theApp = &app;
    /* The sample type to use */
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 2;

    pa_simple *s = NULL;
    int ret = 1;
    int error;

    /* Create the recording stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        cerr<<"pa_simple_new() failed: "<<pa_strerror(error)<<endl;
        return 1;
    }

    //	app.setVerticalSyncEnabled(true);

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

    tex.create(texSize, 1);
    tex.setSmooth(true);

    short rawbuf[MAXFRAME*2];
    float lbuf[MAXFRAME];
    float rbuf[MAXFRAME];
    float mbuf[MAXFRAME];

    FFT fft;

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

            if ((event.type == sf::Event::KeyPressed) && (event.key.code == Keyboard::Space))
            {
                running = !running;
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
        if(running)
            tim += dt;

        frameTime += dt;
        frameCount++;

        int samples = dt*sampleRate-1;
        if(samples > MAXFRAME)
        {
            cerr<<"u sux an u cant has realtime"<<endl<<flush;
            samples = MAXFRAME;
        }
        if (pa_simple_read(s, rawbuf, samples*4, &error) < 0) {
            cerr<<"pa_simple_read() failed: "<<pa_strerror(error)<<endl;
            return 1;
        }

        for(int i = 0; i < samples; i++)
        {
            lbuf[i] = rawbuf[i*2] / float(1<<15);
            rbuf[i] = rawbuf[i*2+1] / float(1<<15);
            mbuf[i] = (lbuf[i] + rbuf[i])/2;
        }

        fft.forward(mbuf, samples);

        for(int i = 0; i < samples; i++)
        {
            float s = mbuf[i];
            s = beatFlt.sample(s);
            s -= 0.9;
            beat = max(beat, s);
        }


        unsigned char fftTex[texSize*4];
        for(int i = 0; i < texSize; i++)
        {
            float v = fft.spectrum[i]*20;
            if(v < 0) v = 0;
            if(v > 1) v = 1;
            fftTex[i*4] = (unsigned char)(v*255);
        }
        tex.update(fftTex);

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
        shader->bind();
        shader->setParameter("time", tim);
        shader->setParameter("beat", beat);

        glBegin(GL_QUADS);
        float aspectRatio = (float)theApp->getSize().x / (float)theApp->getSize().y;
        glTexCoord2f(-aspectRatio, -1); glVertex2f(-1, -1);
        glTexCoord2f(aspectRatio, -1); glVertex2f(1, -1);
        glTexCoord2f(aspectRatio, 1); glVertex2f(1, 1);
        glTexCoord2f(-aspectRatio, 1); glVertex2f(-1, 1);
        glEnd();

        fftShader->bind();
        fftShader->setParameter("fft", tex);

        glBegin(GL_QUADS);
        glTexCoord2f(-1, -1); glVertex2f(-1, -1);
        glTexCoord2f(1, -1); glVertex2f(1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, 1);
        glTexCoord2f(-1, 1); glVertex2f(-1, 1);
        glEnd();

        app.display();
    }

    return EXIT_SUCCESS;
}



void renderQuad()
{

}
