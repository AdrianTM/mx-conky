# **********************************************************************
# * Copyright (C) 2017-2025 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of mx-conky.
# *
# * mx-conky is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * mx-conky is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with mx-conky.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += core gui widgets
CONFIG   += debug_and_release warn_on strict_c++ c++20

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -flto=auto
    QMAKE_LFLAGS += -flto=auto
    QMAKE_CXXFLAGS_RELEASE = -O3
}

# Split compiler flags to avoid MOC issues
QMAKE_CXXFLAGS += -Wpedantic -pedantic 
QMAKE_CXXFLAGS += -Werror=return-type -Werror=switch
QMAKE_CXXFLAGS += -Werror=uninitialized -Werror=return-local-addr

# Only apply -Werror to source compilation, not MOC
!contains(CONFIG, no_werror) {
    QMAKE_CXXFLAGS += -Werror
}

# Handle 32-bit build issues 
linux-g++* {
    equals(QT_ARCH, i386) {
        # Reduce strict flags for 32-bit MOC compatibility
        CONFIG += no_werror
        QMAKE_CXXFLAGS -= -Werror
    }
}

TARGET = mx-conky
TEMPLATE = app

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    cmd.cpp \
    versionnumber.cpp \
    conkyitem.cpp \
    conkymanager.cpp \
    conkylistwidget.cpp \
    settingsdialog.cpp \
    conkycustomizedialog.cpp

HEADERS  += \
    mainwindow.h \
    cmd.h \
    versionnumber.h \
    conkyitem.h \
    conkymanager.h \
    conkylistwidget.h \
    settingsdialog.h \
    conkycustomizedialog.h


TRANSLATIONS += \
    translations/mx-conky_en.ts

RESOURCES += \
    images.qrc


