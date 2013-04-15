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

LIBS += -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lGL -lGLU -lpulse -lpulse-simple
SOURCES += main.cpp
HEADERS += 

OTHER_FILES += \
    vertex.glsl \
    fragment.glsl \
    vertex.glsl \
    fragment.glsl \
    fragment-fft.glsl
