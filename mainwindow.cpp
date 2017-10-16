/**********************************************************************
 *  MainWindow.cpp
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
#include "ui_mainwindow.h"

#include <QColorDialog>


#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// setup versious items first time program runs
void MainWindow::setup()
{
    cmd = new Cmd(this);
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::cleanup);
    version = getVersion("mx-conky");
    this->setWindowTitle(tr("MX Conky"));
    this->adjustSize();
    ui->buttonCancel->setEnabled(true);
}

// cleanup environment when window is closed
void MainWindow::cleanup()
{
    if(!cmd->terminate()) {
        cmd->kill();
    }
}


// Get version of the program
QString MainWindow::getVersion(QString name)
{
    return cmd->getOutput("dpkg -l "+ name + "| awk 'NR==6 {print $3}'");
}

void MainWindow::setColor(QWidget *widget)
{
    QColorDialog *dialog = new QColorDialog();
    QColor color = dialog->getColor(widget->palette().color(QWidget::backgroundRole()));
    QPalette pal = palette();
    pal.setColor(QPalette::Background, color);
    widget->setStyleSheet("");
    widget->setAutoFillBackground(true);
    widget->setPalette(pal);
}

void MainWindow::cmdStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}


void MainWindow::cmdDone()
{
 //   ui->progressBar->setValue(ui->progressBar->maximum());
    setCursor(QCursor(Qt::ArrowCursor));
    cmd->disconnect();
}

// set proc and timer connections
void MainWindow::setConnections()
{
    connect(cmd, &Cmd::started, this, &MainWindow::cmdStart);
    connect(cmd, &Cmd::finished, this, &MainWindow::cmdDone);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Conky"), "<p align=\"center\"><b><h2>" +
                       tr("MX Conky") + "</h2></b></p><p align=\"center\">" + tr("Version: ") + version + "</p><p align=\"center\"><h3>" +
                       tr("GUI program for configuring Conky in MX Linux") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    msgBox.addButton(tr("License"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    if (msgBox.exec() == QMessageBox::AcceptRole) {
        system("mx-viewer file:///usr/share/doc/mx-conky/license.html '" + tr("MX Conky").toUtf8() + " " + tr("License").toUtf8() + "'");
    }
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    this->hide();
    QString cmd = QString("mx-viewer https://mxlinux.org/user_manual_mx15/mxum.html#test '%1'").arg(tr("MX Conky"));
    system(cmd.toUtf8());
    this->show();
}


void MainWindow::on_buttonDefaultColor_clicked()
{
    setColor(ui->widgetDefaultColor);
}

void MainWindow::on_buttonColor1_clicked()
{
    setColor(ui->widgetColor1);
}

void MainWindow::on_buttonColor2_clicked()
{
    setColor(ui->widgetColor2);
}

void MainWindow::on_buttonColor3_clicked()
{
    setColor(ui->widgetColor3);
}
