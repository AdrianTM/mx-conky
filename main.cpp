/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2017 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of mx-conky.
 *
 * mx-conky is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mx-conky is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mx-conky.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "mainwindow.h"
#include <unistd.h>
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QDir>
#include <QFileDialog>
#include <QDebug>

#include <cmd.h>


// return the config file used for the newest conky process
QString getRunningConky()
{
    Cmd cmd;
    return cmd.getOutput("pgrep -xan conky | cut -d' ' -f4");
}

// check conky-manager folder existence
QDir findFolder()
{
    QDir dir = QDir::homePath() + "/.conky";
    QDir dir_old = QDir::homePath() + "/conky-manager";
    if (dir.exists()) {
        return dir;
    } else if (dir_old.exists()) {
        return dir_old;
    } else {
        return QDir();
    }
}

QString openFile(QDir dir)
{
    QFileDialog dialog;
    QString file_name = getRunningConky();
    qDebug() << "File name" << file_name;

    if (file_name != "") {
        return file_name;
    }

    QString selected = dialog.getOpenFileName(0, QObject::tr("Select Conky Manager config file"), dir.path());
    if (selected != "") {
        return selected;
    }
    return "";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon("/usr/share/pixmaps/mx-conky.png"));

    QTranslator qtTran;
    qtTran.load(QString("qt_") + QLocale::system().name());
    a.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("mx-conky_") + QLocale::system().name(), "/usr/share/mx-conky/locale");
    a.installTranslator(&appTran);

    if (system("dpkg -s conky-manager | grep -q 'Status: install ok installed'") != 0 &&
               system("dpkg -s conky-manager2 | grep -q 'Status: install ok installed'" ) != 0) {
        QMessageBox::critical(0, QObject::tr("Error"),
                              QObject::tr("Could not find conky-manager, please install it before running mx-conky"));
        return 1;
    }


    if (getuid() != 0) {
        QDir dir = findFolder();
        if (dir.path() == "." ) {
            QMessageBox::information(0, QObject::tr("Folder not found"),
                                     QObject::tr("Could not find the configuration folder. Will start conky-manager for you in order to create the folder"));
            system("conky-manager");
        }
        dir = findFolder();
        if (dir.path() == "." ) {
            QMessageBox::critical(0, QString::null,
                                  QObject::tr("Could not find the configration folder, giving up"));
            return 1;
        }
        QString file = openFile(dir);
        if (file == "") {
            QMessageBox::critical(0, QObject::tr("Error"),
                                  QObject::tr("No file was selected, quiting program"));
            return 1;
        }

        MainWindow w(0, file);
        w.show();
        return a.exec();

    } else {
        QApplication::beep();
        QMessageBox::critical(0, QString::null,
                              QObject::tr("You must run this program as normal user"));
        return 1;
    }
}
