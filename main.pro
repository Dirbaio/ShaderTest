CONFIG -= qt
QMAKE_CFLAGS += -g -O3
QMAKE_CXXFLAGS += -g -O3

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_LFLAGS -= -O1
QMAKE_LFLAGS += -O3
QMAKE_LFLAGS_RELEASE -= -O1
QMAKE_LFLAGS_RELEASE += -O3
QMAKE_LDFLAGS -= -O1
QMAKE_LDFLAGS += -O3

LIBS += -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lGLEW -lGL -lGLU -lpulse -lpulse-simple
SOURCES += main.cpp \
    record.cpp \
    dsp.cpp
HEADERS += \ 
    record.h \
    dsp.h

OTHER_FILES += \
    vertex.glsl \
    fragment.glsl \
    vertex.glsl \
    fragment.glsl \
    fragment-fft.glsl \
    compute.comp
