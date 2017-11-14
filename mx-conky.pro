# **********************************************************************
# * Copyright (C) 2017 MX Authors
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

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-conky
TEMPLATE = app


SOURCES += main.cpp\
    cmd.cpp \
    mainwindow.cpp \
    versionnumber.cpp

HEADERS  += \
    cmd.h \
    mainwindow.h \
    versionnumber.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-conky_ca.ts \
                translations/mx-conky_de.ts \
                translations/mx-conky_el.ts \
                translations/mx-conky_es.ts \
                translations/mx-conky_fr.ts \
                translations/mx-conky_it.ts \
                translations/mx-conky_ja.ts \
                translations/mx-conky_nl.ts \
                translations/mx-conky_ro.ts \
                translations/mx-conky_sv.ts

RESOURCES += \
    images.qrc


