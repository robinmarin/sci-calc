QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = scicalc_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/engine.h ../src/theme.h ../src/translator.h
SOURCES += scicalc_tests.cpp ../src/backend.cpp ../src/engine.cpp ../src/theme.cpp ../src/translator.cpp
