#include "dsp.h"
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

FFT::FFT()
{
    int limit = 1;
    int bit = fftSize >> 1;
    this->reverseTable[0] = 0; // ?!?!?
    while ( limit < fftSize ) {
        cout<<limit<<" "<<bit<<" "<<fftSize<<endl;
        for ( int i = 0; i < limit; i++ )
            this->reverseTable[i + limit] = this->reverseTable[i] + bit;

        limit = limit << 1;
        bit = bit >> 1;
    }

    for ( int i = 0; i < fftSize; i++ ) {
        this->sinTable[i] = sin(-M_PI/i);
        this->cosTable[i] = cos(-M_PI/i);
    }
}

void FFT::forward(float* buffer, int bufferSize)
{
    for(int i = 0; i < fftSize - bufferSize; i++)
        samples[i] = samples[i+bufferSize];

    int x = min(fftSize, bufferSize);
    for(int i = 0; i < x; i++)
        samples[fftSize-x+i] = buffer[bufferSize-x+i];

    forward(samples);
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


const float LN2 = 0.69314718055994530942;

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
        omega = 2 * M_PI * freq / sampleRate;
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
