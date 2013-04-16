#ifndef DSP_H
#define DSP_H

#define fftSize (1<<14)
#define sampleRate 44100.0

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

#endif // DSP_H
