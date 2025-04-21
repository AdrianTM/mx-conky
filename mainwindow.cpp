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

#include <QColorDialog>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextEdit>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent, const QString &file)
    : QDialog(parent),
      ui(new Ui::MainWindow),
      file_name {file},
      capture_lua_color {
          R"(^(?<before>(.*\]\])?\s*(?<color_item>default_color|color\d)(?:\s*=\s*[\"\']))(?:#?)(?<color_value>[[:alnum:]]+)(?<after>(?:[\"\']).*))"},
      capture_old_color {
          R"(^(?<before>\s*(?<color_item>default_color|color\d)(?:\s+))(?:#?)(?<color_value>[[:alnum:]]+)(?<after>.*))"},
      lua_comment_end {R"(^\s*\]\])"},
      lua_comment_line {R"(^\s*--)"},
      lua_comment_start {R"(^\s*--\[\[)"},
      lua_config {R"(^\s*(conky.config\s*=\s*{)"},
      lua_format {R"(^\s*(--|conky.config\s*=\s*{|conky.text\s*=\s*{))"},
      old_comment_line {R"(^\s*#)"},
      old_format {R"(^\s*#|^TEXT$)"}
{
    debug = !QProcessEnvironment::systemEnvironment().value("DEBUG").isEmpty();

    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << QCoreApplication::applicationVersion();
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // Enable close, min, and max buttons
    setWindowTitle(tr("MX Conky"));
    refresh();
    restoreGeometry(settings.value("geometry").toByteArray());
    setConnections();
}

MainWindow::~MainWindow()
{
    saveBackup();
    delete ui;
}

// Detect conky format (old or lua)
void MainWindow::detectConkyFormat()
{
    QRegularExpression lua_format_regexp(lua_format);
    QRegularExpression old_format_regexp(old_format);
    const QStringList list = file_content.split('\n');

    if (debug) {
        qDebug() << "Detecting conky format: " << file_name;
    }

    conky_format_detected = false;

    for (const QString &row : list) {
        if (lua_format_regexp.match(row).hasMatch()) {
            is_lua_format = true;
            conky_format_detected = true;
            if (debug) {
                qDebug() << "Conky format detected: 'lua-format' in " << file_name;
            }
            return;
        }
        if (old_format_regexp.match(row).hasMatch()) {
            is_lua_format = false;
            conky_format_detected = true;
            if (debug) {
                qDebug() << "Conky format detected: 'old-format' in " << file_name;
            }
            return;
        }
    }
}

// Find defined colors in the config file
void MainWindow::parseContent()
{
    QRegularExpression regexp_color
        = is_lua_format ? QRegularExpression(capture_lua_color) : QRegularExpression(capture_old_color);
    QString comment_sep = is_lua_format ? "--" : "#";
    bool own_window_hints_found = false;
    const QStringList list = file_content.split('\n', Qt::SkipEmptyParts);

    if (debug) {
        qDebug() << "Parsing content: " + file_name;
    }

    bool lua_block_comment = false;
    //  Bool lua_config = false;
    for (const QString &row : list) {
        // Lua comment block
        QString trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) {
                        qDebug() << "Lua block comment end 'ENDS WITH LINE' found";
                    }
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow;
                    }
                } else {
                    continue;
                }
            }
            if (!lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) {
                        qDebug() << "Lua block comment 'STARTS WITH LINE' found";
                    }
                    lua_block_comment = true;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow;
                    }
                }
            }
        }
        // Comment line
        if (trow.startsWith(comment_sep)) {
            continue;
        }

        // Remove inline comments
        if (trow.contains(comment_sep)) {
            trow = trow.split(comment_sep).first().trimmed();
        }

        // Match color lines
        auto match_color = regexp_color.match(trow);
        if (match_color.hasMatch()) {
            const QString color_item = match_color.captured("color_item");
            const QString color_value = match_color.captured("color_value");
            QWidget *colorWidget = nullptr;

            if (color_item == "default_color") {
                colorWidget = ui->widgetDefaultColor;
            } else if (color_item.startsWith("color")) {
                const int index = color_item.mid(5).toInt(); // Extract the number from "colorX";

                colorWidget = ui->groupBoxColors->findChild<QWidget *>(QString("widgetColor%1").arg(index));
                if (colorWidget) {
                    QLabel *label = ui->groupBoxColors->findChild<QLabel *>(QString("labelColor%1").arg(index));
                    if (label) {
                        label->setText(QString("Color%1").arg(index));
                    }
                }
            }
            if (colorWidget) {
                setColor(colorWidget, strToColor(color_value));
            }
            continue;
        }

        // Handle desktop configuration
        if (trow.startsWith("own_window_hints")) {
            own_window_hints_found = true;
            if (debug) {
                qDebug() << "own_window_hints line found:" << trow;
            }
            ui->radioAllDesktops->setChecked(trow.contains("sticky"));
            ui->radioDesktop1->setChecked(!trow.contains("sticky"));
            continue;
        }

        // Set day/month format
        if (trow.contains("%A")) {
            ui->radioDayLong->setChecked(true);
        } else if (trow.contains("%a")) {
            ui->radioDayShort->setChecked(true);
        }

        if (trow.contains("%B")) {
            ui->radioMonthLong->setChecked(true);
        } else if (trow.contains("%b")) {
            ui->radioMonthShort->setChecked(true);
        }
    }

    if (!own_window_hints_found) {
        ui->radioDesktop1->setChecked(true);
    }
}

bool MainWindow::checkConkyRunning()
{
    QProcess process;
    process.start("pgrep", QStringList() << "-u" << qgetenv("USER") << "-x"
                                         << "conky");
    process.waitForFinished();
    int ret = process.exitCode();
    if (debug) {
        qDebug() << "pgrep -u" << qgetenv("USER") << "-x conky :" << ret;
    }
    bool isRunning = (ret == 0);
    ui->pushToggleOn->setText(isRunning ? "Stop" : "Run");
    ui->pushToggleOn->setIcon(QIcon::fromTheme(isRunning ? "stop" : "start"));
    return isRunning;
}

// Read config file
bool MainWindow::readFile(const QString &file_name)
{
    QFile file(file_name);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file: " << file.fileName();
        return false;
    }
    file_content = file.readAll().trimmed();
    file.close();
    return true;
}

QColor MainWindow::strToColor(const QString &colorstr)
{
    QColor color(colorstr);
    if (!color.isValid()) { // if color is invalid assume RGB values and add a # in front of the string
        color.setNamedColor('#' + colorstr);
    }
    return color;
}

void MainWindow::refresh()
{
    modified = false;

    // Hide all color frames by default, only show those specified in the config file
    const QList<QWidget *> frames = {ui->frameDefault, ui->frame0, ui->frame1, ui->frame2, ui->frame3, ui->frame4,
                                     ui->frame5,       ui->frame6, ui->frame7, ui->frame8, ui->frame9};
    for (QWidget *frame : frames) {
        frame->hide();
    }

    // Set a border around color widgets for better visibility
    const QList<QWidget *> colorWidgets = {ui->widgetDefaultColor, ui->widgetColor0, ui->widgetColor1, ui->widgetColor2,
                                           ui->widgetColor3,       ui->widgetColor4, ui->widgetColor5, ui->widgetColor6,
                                           ui->widgetColor7,       ui->widgetColor8, ui->widgetColor9};
    for (QWidget *widget : colorWidgets) {
        widget->setStyleSheet("border: 1px solid black");
    }

    // Update the button text to reflect the current conky file name
    ui->pushChange->setText(QFileInfo(file_name).fileName());

    checkConkyRunning();

    // Backup the current configuration file
    QFile::remove(file_name + ".bak");
    QFile::copy(file_name, file_name + ".bak");

    // Read the configuration file and parse its content
    if (readFile(file_name)) {
        detectConkyFormat();
        parseContent();
    }
    adjustSize();
}

void MainWindow::setConnections()
{
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushCM, &QPushButton::clicked, this, &MainWindow::pushCM_clicked);
    connect(ui->pushChange, &QPushButton::clicked, this, &MainWindow::pushChange_clicked);
    connect(ui->pushDefaultColor, &QPushButton::clicked, this, &MainWindow::pushDefaultColor_clicked);
    connect(ui->pushEdit, &QPushButton::clicked, this, &MainWindow::pushEdit_clicked);
    connect(ui->pushRestore, &QPushButton::clicked, this, &MainWindow::pushRestore_clicked);
    connect(ui->pushToggleOn, &QPushButton::clicked, this, &MainWindow::pushToggleOn_clicked);
    connect(ui->radioAllDesktops, &QRadioButton::clicked, this, &MainWindow::radioAllDesktops_clicked);
    connect(ui->radioDayLong, &QRadioButton::clicked, this, &MainWindow::radioDayLong_clicked);
    connect(ui->radioDayShort, &QRadioButton::clicked, this, &MainWindow::radioDayShort_clicked);
    connect(ui->radioDesktop1, &QRadioButton::clicked, this, &MainWindow::radioDesktop1_clicked);
    connect(ui->radioMonthLong, &QRadioButton::clicked, this, &MainWindow::radioMonthLong_clicked);
    connect(ui->radioMonthShort, &QRadioButton::clicked, this, &MainWindow::radioMonthShort_clicked);
    for (int i = 0; i < 10; ++i) {
        auto *colorButton = ui->groupBoxColors->findChild<QToolButton *>(QString("pushColor%1").arg(i));
        if (colorButton) {
            connect(colorButton, &QToolButton::clicked, this, [this, i]() { pushColorButton_clicked(i); });
        }
    }
}

void MainWindow::saveBackup()
{
    if (!modified) {
        return;
    }

    int ans = QMessageBox::question(this, tr("Backup Config File"), tr("Do you want to preserve the original file?"));
    if (ans == QMessageBox::Yes) {
        QString time_stamp = QDateTime::currentDateTime().toString("yyMMdd_HHmmss");
        QFileInfo fi(file_name);
        QString new_name = fi.canonicalPath() + '/' + fi.baseName() + '_' + time_stamp;

        if (!fi.completeSuffix().isEmpty()) {
            new_name += '.' + fi.completeSuffix();
        }

        if (QFile::copy(file_name + ".bak", new_name)) {
            QMessageBox::information(this, tr("Backed Up Config File"),
                                     tr("The original configuration was backed up to %1").arg(new_name));
        } else {
            QMessageBox::warning(this, tr("Backup Failed"), tr("Failed to create a backup file."));
        }
    }

    QFile::remove(file_name + ".bak");
}

// Write color change back to the file
void MainWindow::writeColor(QWidget *widget, const QColor &color)
{
    QRegularExpression regexp_color
        = is_lua_format ? QRegularExpression(capture_lua_color) : QRegularExpression(capture_old_color);
    QString comment_sep = is_lua_format ? "--" : "#";

    QString item_name
        = (widget->objectName() == "widgetDefaultColor")
              ? "default_color"
              : (widget->objectName().startsWith("widgetColor") ? QString("color%1").arg(widget->objectName().mid(11))
                                                                : QString());

    const QStringList list = file_content.split('\n');
    QStringList new_list;
    new_list.reserve(list.size());
    bool lua_block_comment = false;

    for (const QString &row : list) {
        QString trow = row.trimmed();

        // Handle Lua block comments
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) {
                        qDebug() << "Lua block comment end 'ENDS WITH LINE' found";
                    }
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow;
                    }
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (!lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) {
                        qDebug() << "Lua block comment 'STARTS WITH LINE' found";
                    }
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow;
                    }
                }
            }
        }
        // Comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }

        // Match and update color lines
        auto match_color = regexp_color.match(row);
        if (match_color.hasMatch() && match_color.captured("color_item") == item_name) {
            QString color_name = is_lua_format ? color.name() : color.name().remove('#');
            new_list << match_color.captured("before") + color_name + match_color.captured("after");
        } else {
            new_list << row;
        }
    }

    file_content = new_list.join('\n') + '\n';
    writeFile(QFile(file_name), file_content);
}

void MainWindow::writeFile(QFile file, const QString &content)
{
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Error opening file " + file_name + " for output";
        return;
    }

    QTextStream out(&file);
    out << content;
    modified = true;
    file.close();
}

void MainWindow::pickColor(QWidget *widget)
{
    QColor initialColor = widget->palette().color(QWidget::backgroundRole());
    QColor selectedColor = QColorDialog::getColor(initialColor, this, tr("Select Color"));

    if (selectedColor.isValid()) {
        setColor(widget, selectedColor);
        writeColor(widget, selectedColor);
    } else {
        qDebug() << "Color selection was canceled or invalid.";
    }
}

void MainWindow::setColor(QWidget *widget, const QColor &color)
{
    if (widget && color.isValid()) {
        widget->parentWidget()->show();
        QPalette pal = widget->palette();
        pal.setColor(QPalette::Window, color);
        widget->setAutoFillBackground(true);
        widget->setPalette(pal);
    } else {
        qDebug() << "Invalid widget or color.";
    }
}

void MainWindow::pushAbout_clicked()
{
    hide();
    const QString url = "file:///usr/share/doc/mx-conky/license.html";
    QMessageBox msgBox(QMessageBox::NoIcon, tr("About MX Conky"),
                       "<p align=\"center\"><b><h2>" + tr("MX Conky") + "</h2></b></p><p align=\"center\">"
                           + tr("Version: ") + QCoreApplication::applicationVersion() + "</p><p align=\"center\"><h3>"
                           + tr("GUI program for configuring Conky in MX Linux")
                           + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br "
                             "/></p><p align=\"center\">"
                           + tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    auto *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    auto *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    auto *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        const QString executablePath = QStandardPaths::findExecutable("mx-viewer");
        const QString cmd_str = executablePath.isEmpty() ? "xdg-open" : "mx-viewer";
        QProcess::startDetached(cmd_str, {url});
    } else if (msgBox.clickedButton() == btnChangelog) {
        auto *changelog = new QDialog(this);
        changelog->setWindowTitle(tr("Changelog"));
        const auto height {500};
        const auto width {600};
        changelog->resize(width, height);

        auto *text = new QTextEdit;
        text->setReadOnly(true);
        text->setText(Cmd().getCmdOut(
            "zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName() + "/changelog.gz"));

        auto *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        auto *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    show();
}

void MainWindow::pushHelp_clicked()
{
    QString url = "/usr/share/doc/mx-conky/mx-conky.html";
    QString cmd_str = system("command -v mx-viewer") == 0 ? "mx-viewer " + url + " " + tr("MX Conky Help") + "&"
                                                          : "xdg-open " + url;
    system(cmd_str.toUtf8());
}

void MainWindow::pushColorButton_clicked(int colorIndex)
{
    QWidget *colorWidget = (colorIndex >= 0 && colorIndex < 10)
                               ? ui->groupBoxColors->findChild<QWidget *>(QString("widgetColor%1").arg(colorIndex))
                               : ui->widgetDefaultColor;

    if (colorWidget) {
        pickColor(colorWidget);
    } else {
        qWarning() << "Color widget not found for index:" << colorIndex;
    }
}

void MainWindow::pushDefaultColor_clicked()
{
    pushColorButton_clicked(-1); // Default color
}

void MainWindow::pushToggleOn_clicked()
{
    QString cmd_str = checkConkyRunning() ? "pkill -u $(id -nu) -x conky"
                                          : QString("cd \"$(dirname '%1')\"; conky -c '%1' &").arg(file_name);
    system(cmd_str.toUtf8());
    checkConkyRunning();
}

void MainWindow::pushRestore_clicked()
{
    QString backupFileName = file_name + ".bak";
    if (QFile::exists(backupFileName)) {
        QFile::remove(file_name);
        if (QFile::copy(backupFileName, file_name)) {
            refresh();
        } else {
            qWarning() << "Failed to restore from backup:" << backupFileName;
        }
    } else {
        QMessageBox::warning(this, tr("Restore Failed"), tr("Backup file does not exist."));
    }
}

void MainWindow::pushEdit_clicked()
{
    hide();
    QString run = "set -o pipefail; ";
    run += "XDHD=${XDG_DATA_HOME:-$HOME/.local/share}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}; ";
    run += "eval grep -sh -m1 ^Exec {${XDHD//://applications/,}/applications/}";
    run += "$(xdg-mime query default text/plain)  2>/dev/null ";
    run += "| head -1 | sed 's/^Exec=//' | tr -d '\"' | tr -s ' ' | sed 's/@@//g; s/%f//I; s/%u//I' ";
    bool quiet = true;
    if (debug) {
        qDebug() << "run-cmd: " << run;
        quiet = false;
    }
    QString editor;
    bool error = Cmd().run(run, editor, quiet);
    if (debug) {
        qDebug() << "run:'" + editor + " '" + file_name.toUtf8() + "'";
    }
    if (editor.startsWith("kate -s") || editor.startsWith("kate --start")) {
        editor = "kate";
    }
    if (system(editor.toUtf8() + " '" + file_name.toUtf8() + "'") != 0) {
        if (error || (system("which " + editor.toUtf8() + " 1>/dev/null") != 0)) {
            if (debug) {
                qDebug() << "no default text editor defined" << editor;
            }
            // try featherpad explicitly
            if (system("which featherpad 1>/dev/null") == 0) {
                if (debug) {
                    qDebug() << "try featherpad text editor ";
                }
                system("featherpad '" + file_name.toUtf8() + "'");
            }
            refresh();
            show();
            return;
        }
    }
    refresh();
    show();
}

void MainWindow::pushChange_clicked()
{
    saveBackup();
    QString selected = QFileDialog::getOpenFileName(nullptr, QObject::tr("Select Conky Manager config file"),
                                                    QFileInfo(file_name).path());
    if (!selected.isEmpty()) {
        file_name = selected;
    }
    refresh();
}

void MainWindow::radioDesktop1_clicked()
{
    QRegularExpression regexp_lua_owh(capture_lua_owh);
    QRegularExpression regexp_old_owh(capture_old_owh);
    QRegularExpression regexp_owh = is_lua_format ? regexp_lua_owh : regexp_old_owh;
    QString comment_sep = is_lua_format ? "--" : "#";
    QRegularExpressionMatch match_owh;

    bool lua_block_comment = false;

    const QStringList list = file_content.split('\n');
    QStringList new_list;
    new_list.reserve(list.size());
    for (const QString &row : list) {
        // Lua comment block
        QString trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) {
                        qDebug() << "Lua block comment end 'ENDS WITH LINE' found";
                    }
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow;
                    }
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (!lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) {
                        qDebug() << "Lua block comment 'STARTS WITH LINE' found";
                    }
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow;
                    }
                }
            }
        }
        // Comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }

        if (!trow.startsWith("own_window_hints")) {
            new_list << row;
            continue;
        } else {

            if (debug) {
                qDebug() << "on_radioDesktops1_clicked: own_window_hints found row : " << row;
                qDebug() << "on_radioDesktops1_clicked: own_window_hints found trow: " << trow;
            }
            match_owh = regexp_owh.match(row);
            if (match_owh.hasMatch() && match_owh.captured("item") == "own_window_hints") {
                QString owh_value = match_owh.captured("value");
                owh_value.replace(",sticky", "");
                owh_value.replace("sticky", "");
                QString new_row = match_owh.captured("before") + owh_value + match_owh.captured("after");
                if (debug) {
                    qDebug() << "Removed sticky: " << new_row;
                }
                new_list << new_row;
            } else {
                if (debug) {
                    qDebug() << "ERROR : " << row;
                }
                if (is_lua_format) {
                    if (debug) {
                        qDebug() << "regexp_owh : " << regexp_lua_owh;
                        qDebug() << "regexp_owh : " << regexp_owh;
                    }
                } else {
                    if (debug) {
                        qDebug() << "regexp_owh : " << regexp_old_owh;
                    }
                }
                new_list << row;
            }
        }
    }

    file_content = new_list.join('\n').append('\n');
    writeFile(QFile(file_name), file_content);
}

void MainWindow::radioAllDesktops_clicked()
{
    QString comment_sep;
    QRegularExpression regexp_lua_owh(capture_lua_owh);
    QRegularExpression regexp_old_owh(capture_old_owh);
    QRegularExpression regexp_owh;
    QRegularExpressionMatch match_owh;
    QString conky_config_lua_end = "}";
    QString conky_config_old_end = "TEXT";
    QString conky_config_end;
    QString conky_sticky;
    bool lua_block_comment {false};
    bool conky_config {false};

    if (is_lua_format) {
        regexp_owh = regexp_lua_owh;
        comment_sep = "--";
        conky_config = false;
        conky_config_end = conky_config_lua_end;
        conky_sticky = "\town_window_hints = 'sticky',";
    } else {
        regexp_owh = regexp_old_owh;
        comment_sep = "#";
        conky_config = true;
        conky_config_end = conky_config_old_end;
        conky_sticky = "own_window_hints sticky";
    }

    bool found = false;
    const QStringList list = file_content.split('\n');
    QStringList new_list;
    QString trow;
    for (const QString &row : list) {
        trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) {
                        qDebug() << "Lua block comment end 'ENDS WITH LINE' found";
                    }
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow;
                    }
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (!lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) {
                        qDebug() << "Lua block comment 'STARTS WITH LINE' found";
                    }
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) {
                        qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow;
                    }
                }
            }
        }
        // comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }
        if (is_lua_format && trow.startsWith("conky.config")) {
            conky_config = true;
        }

        if (!found && conky_config && trow.startsWith(conky_config_end)) {
            conky_config = false;
            new_list << conky_sticky;
            new_list << row;
            continue;
        }
        if (!conky_config) {
            new_list << row;
            continue;
        }

        if (!trow.startsWith("own_window_hints ")) {
            new_list << row;
            continue;
        } else {
            if (debug) {
                qDebug() << "on_radioAllDesktops_clicked: own_window_hints found row : " << row;
                qDebug() << "on_radioAllDesktops_clicked: own_window_hints found trow: " << trow;
            }
            match_owh = regexp_owh.match(row);
            if (match_owh.hasMatch() && match_owh.captured("item") == "own_window_hints") {
                QString owh_value = match_owh.captured("value");
                if (owh_value.length() == 0) {
                    owh_value = "sticky";
                } else {
                    owh_value.append(",sticky");
                }
                QString new_row = match_owh.captured("before") + owh_value + match_owh.captured("after");
                if (debug) {
                    qDebug() << "Append sticky: " << new_row;
                }

                new_list << match_owh.captured("before") + owh_value + match_owh.captured("after");
                found = true;
            } else {
                if (debug) {
                    qDebug() << "ERROR : " << row;
                }
                if (is_lua_format) {
                    if (debug) {
                        qDebug() << "regexp_owh : " << regexp_lua_owh;
                    }
                } else {
                    if (debug) {
                        qDebug() << "regexp_owh : " << regexp_old_owh;
                    }
                }
                found = false;
                new_list << row;
            }
        }
    }

    file_content = new_list.join('\n').append('\n');
    writeFile(file_name, file_content);
}

void MainWindow::radioDayLong_clicked()
{
    file_content.replace("%a", "%A");
    writeFile(file_name, file_content);
}

void MainWindow::radioDayShort_clicked()
{
    file_content.replace("%A", "%a");
    writeFile(file_name, file_content);
}

void MainWindow::radioMonthLong_clicked()
{
    file_content.replace("%b", "%B");
    writeFile(file_name, file_content);
}

void MainWindow::radioMonthShort_clicked()
{
    file_content.replace("%B", "%b");
    writeFile(file_name, file_content);
}

void MainWindow::pushCM_clicked()
{
    QString command = "command -v conky-manager && conky-manager || command -v conky-manager2 && conky-manager2";
    QProcess::startDetached("bash", {"-c", command});
}

void MainWindow::closeEvent(QCloseEvent * /*event*/)
{
    settings.setValue("geometry", saveGeometry());
}
