#-------------------------------------------------
#
# Project created by QtCreator 2014-07-12T20:11:20
#
#-------------------------------------------------

QT       += core gui sql
CONFIG += c++11
LIBS += -lws2_32

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Exporter
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp

HEADERS  += widget.h

FORMS    += widget.ui
