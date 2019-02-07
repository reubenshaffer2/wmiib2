#-------------------------------------------------
#
# Project created by QtCreator 2019-02-02T04:45:32
#
#-------------------------------------------------

# Copyright 2019 Reuben Robert Shaffer II.  All rights reserved.

# This file is part of WMIIB2.

# WMIIB2 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# WMIIB2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with WMIIB2.  If not, see <https://www.gnu.org/licenses/>.



QT       += core gui x11extras

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = wmiib2
TEMPLATE = app
LIBS += -lxcb -lxcb-composite -lxcb-damage

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        wmiib2.cpp \
    xcbeventfilter.cpp \
    atomcache.cpp \
    wininfo.cpp \
    settingswindow.cpp

HEADERS += \
        wmiib2.h \
    xcbeventfilter.h \
    atomcache.h \
    wininfo.h \
    settingswindow.h

FORMS += \
        wmiib2.ui \
    settingswindow.ui

DISTFILES += \
    TODO \
    README \
    LICENSE \
    COPYING

RESOURCES += \
    resources.qrc
