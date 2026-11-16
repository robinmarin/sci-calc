QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = scicalc
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/engine.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/engine.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
