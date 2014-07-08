#-------------------------------------------------
#
# Project created by QtCreator 2014-06-21T19:41:43
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = ujea
CONFIG   += console c++11
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    jobsexecuter.cpp \
    sysinfo.cpp

include(qamqp/src/qamqp/qamqp.pri)

HEADERS += \
    jobsexecuter.h \
    sysinfo.h

isEmpty( PREFIX ){
  PREFIX =/usr
}

target.path += $${PREFIX}/bin/

upstart.files = ujea.conf
upstart.path = /etc/init

default.path = /etc/default
default.extra = install -m 0644 -D ujea.default $(INSTALL_ROOT)/etc/default/ujea

INSTALLS += target upstart default

