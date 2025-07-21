/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2017-2025 MX Authors
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

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include "cmd.h"
#include "mainwindow.h"
#include "version.h"
#include "versionnumber.h"
#include <unistd.h>

// Return the config file used for the newest conky process
QString getRunningConky()
{
    const QString pid = Cmd().getCmdOut("pgrep -u $(id -nu) -nx conky", true);
    if (pid.isEmpty()) {
        return {};
    }
    const QString conkyrc = Cmd().getCmdOutUntrimmed(
        QString("sed -nrz '/^--config=/s///p; /^(-c|--config)/{n;p;}' /proc/%1/cmdline").arg(pid), true);
    if (conkyrc.startsWith("/")) {
        return conkyrc;
    }
    const QString conkywd
        = Cmd().getCmdOutUntrimmed(QString("sed -nrz '/^PWD=/s///p' /proc/%1/environ").arg(pid), true);
    return conkywd + "/" + conkyrc;
}

QString openFile(const QDir &dir)
{
    const QString file_name = getRunningConky();
    if (!file_name.isEmpty()) {
        return file_name;
    }

    const QString selected
        = QFileDialog::getOpenFileName(nullptr, QObject::tr("Select Conky Manager config file"), dir.path());
    if (!selected.isEmpty()) {
        return selected;
    }
    return {};
}

void updateThemes()
{
    const VersionNumber current_version {Cmd().getCmdOut("dpkg -l mx-conky-data | awk 'NR==6 {print $3}'", true)};

    QSettings settings;
    const QString version = settings.value("data-version").toString();
    VersionNumber recorded_version {version};

    const QString title = QObject::tr("Conky Data Update");
    const QString message = QObject::tr("The MX Conky data set has been updated. <p><p>\
                                  Copy from the folder where it is located <a href=\"/usr/share/mx-conky-data/themes\">/usr/share/mx-conky-data/themes</a> \
                                  whatever you wish to your Home hidden conky folder <a href=\"%1/.conky\">~/.conky</a>. \
                                  Be careful not to overwrite any conkies you have changed.")
                                .arg(QDir::homePath());

    QDir dir {QDir::homePath() + "/.conky"};
    if (recorded_version.toString().isEmpty() || current_version > recorded_version || !dir.exists()) {
        if (!dir.exists()) {
            dir.mkdir(dir.path());
        }
        // Copy the mx-conky-data themes to the default folder
        QProcess copyProcess;
        copyProcess.setProgram("cp");
        copyProcess.setArguments(QStringList() << "--recursive" << "--no-clobber"
                                               << "/usr/share/mx-conky-data/themes/*" << dir.path());
        copyProcess.start();

        if (copyProcess.waitForFinished(10000)) {
            qDebug() << "main: Copy process finished with exit code:" << copyProcess.exitCode();
        } else {
            qDebug() << "main: Copy process timed out";
            copyProcess.kill();
            copyProcess.waitForFinished(1000);
        }
        settings.setValue("data-version", current_version.toString());
        QMessageBox::information(nullptr, title, message);
    }
}

int main(int argc, char *argv[])
{
    // Set Qt platform to XCB (X11) if not already set and we're in X11/WSL environment
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (!qEnvironmentVariableIsEmpty("DISPLAY") || !qEnvironmentVariableIsEmpty("WSL_DISTRO_NAME")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }

    QApplication app(argc, argv);

    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setOrganizationName("MX-Linux");
    QApplication::setApplicationVersion(VERSION);

    QTranslator qtTran;
    if (qtTran.load("qt_" + QLocale().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTran);
    }

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtBaseTran);
    }

    QTranslator appTran;
    if (appTran.load(QApplication::applicationName() + "_" + QLocale().name(),
                     "/usr/share/" + QApplication::applicationName() + "/locale")) {
        QApplication::installTranslator(&appTran);
    }

    if (getuid() != 0) {
        updateThemes();

        MainWindow w;
        w.show();
        return QApplication::exec();

    } else {
        QApplication::beep();
        QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("You must run this program as normal user"));
        return EXIT_FAILURE;
    }
}
