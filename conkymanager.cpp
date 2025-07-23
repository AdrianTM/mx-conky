/**********************************************************************
 *  ConkyManager.cpp
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

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

#include "cmd.h"
#include "conkymanager.h"
#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

ConkyManager::ConkyManager(QObject *parent)
    : QObject(parent),
      m_statusProcess(new QProcess(this)),
      m_autostartTimer(new QTimer(this)),
      m_statusTimer(new QTimer(this)),
      m_statusCheckRunning(false),
      m_startupDelay(20)
{
    m_statusTimer->setInterval(2s);
    connect(m_statusTimer, &QTimer::timeout, this, &ConkyManager::updateRunningStatus);
    m_statusTimer->start();

    m_autostartTimer->setSingleShot(true);
    connect(m_autostartTimer, &QTimer::timeout, this, &ConkyManager::onAutostartTimer);

    // Setup async status process
    connect(m_statusProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &ConkyManager::onStatusProcessFinished);

    loadSettings();
}

ConkyManager::~ConkyManager()
{
    saveSettings();
    clearConkyItems();
}

void ConkyManager::addSearchPath(const QString &path)
{
    if (!m_searchPaths.contains(path)) {
        m_searchPaths.append(path);
        scanForConkies();
        emit conkyItemsChanged();
    }
}

void ConkyManager::removeSearchPath(const QString &path)
{
    if (m_searchPaths.removeOne(path)) {
        scanForConkies();
        emit conkyItemsChanged();
    }
}

QStringList ConkyManager::searchPaths() const
{
    return m_searchPaths;
}

void ConkyManager::scanForConkies()
{
    clearConkyItems();

    QMap<QString, QStringList> conkyFolders; // folderName -> list of fullPaths
    QString userConkyPath = QDir::homePath() + "/.conky";

    // First, collect all conky folders from all search paths
    for (const QString &path : m_searchPaths) {
        QDir dir(path);
        if (!dir.exists()) {
            continue;
        }

        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
        for (const QString &subdir : subdirs) {
            QString fullPath = dir.absoluteFilePath(subdir);

            // Collect all paths for each folder name
            if (!conkyFolders.contains(subdir)) {
                conkyFolders[subdir] = QStringList();
            }
            conkyFolders[subdir].append(fullPath);
        }
    }

    // Now scan folders with priority logic for "Default" view
    // but keep all versions for filtering
    for (auto it = conkyFolders.begin(); it != conkyFolders.end(); ++it) {
        const QStringList &paths = it.value();

        // Sort paths to prioritize ~/.conky first
        QStringList sortedPaths = paths;
        std::sort(sortedPaths.begin(), sortedPaths.end(), [&userConkyPath](const QString &a, const QString &b) {
            bool aIsUser = a.startsWith(userConkyPath);
            bool bIsUser = b.startsWith(userConkyPath);

            // ~/.conky paths come first
            if (aIsUser && !bIsUser) {
                return true;
            }
            if (!aIsUser && bIsUser) {
                return false;
            }
            return a < b; // Alphabetical for same type
        });

        // Scan all versions - the filtering will be done at display level
        for (const QString &path : sortedPaths) {
            scanConkyDirectory(path);
        }
    }

    // Sort conky items by folder name
    std::sort(m_conkyItems.begin(), m_conkyItems.end(), [](const ConkyItem *a, const ConkyItem *b) {
        QString folderA = QFileInfo(a->directory()).fileName().toLower();
        QString folderB = QFileInfo(b->directory()).fileName().toLower();
        return folderA < folderB;
    });

    emit conkyItemsChanged();
}

void ConkyManager::addConkyItemsFromDirectory(const QString &directoryPath)
{
    if (!QFileInfo::exists(directoryPath) || !QFileInfo(directoryPath).isDir()) {
        return;
    }

    // Scan the specific directory and add any new items
    scanConkyDirectory(directoryPath);

    // Sort conky items by folder name (maintain consistent ordering)
    std::sort(m_conkyItems.begin(), m_conkyItems.end(), [](const ConkyItem *a, const ConkyItem *b) {
        QString folderA = QFileInfo(a->directory()).fileName().toLower();
        QString folderB = QFileInfo(b->directory()).fileName().toLower();
        return folderA < folderB;
    });

    emit conkyItemsChanged();
}

QList<ConkyItem *> ConkyManager::conkyItems() const
{
    return m_conkyItems;
}

void ConkyManager::startConky(ConkyItem *item)
{
    if (!item || item->isRunning()) {
        return;
    }

    // Use startDetached to ensure conky remains running after app closes
    QString program = "conky";
    QStringList arguments;
    arguments << "-c" << item->filePath();

    bool started = QProcess::startDetached(program, arguments, item->directory());

    if (!started) {
        return;
    }
    item->setRunning(true);
    emit conkyStarted(item);
}

void ConkyManager::stopConky(ConkyItem *item)
{
    if (!item || !item->isRunning()) {
        return;
    }

    QString pid = getConkyProcess(item->filePath());
    if (!pid.isEmpty()) {
        // Use synchronous approach to avoid lingering processes
        QProcess process;
        process.setProgram("kill");
        process.setArguments(QStringList() << pid);
        process.start();

        if (process.waitForFinished(3000)) {
            // Process killed successfully or failed
        } else {
            // Timeout - force kill the kill process
            process.kill();
            process.waitForFinished(1000);
        }

        item->setRunning(false);
        emit conkyStopped(item);
    }
}

void ConkyManager::removeConkyItem(ConkyItem *item)
{
    if (!item) {
        return;
    }

    // Stop the conky if it's running
    if (item->isRunning()) {
        stopConky(item);
    }

    // Remove from the list
    m_conkyItems.removeAll(item);

    // Delete the item
    item->deleteLater();

    // Emit signal to update UI
    emit conkyItemsChanged();
}

void ConkyManager::startAutostart()
{
    // Start all enabled conkies immediately
    for (ConkyItem *item : m_conkyItems) {
        if (item->isAutostart()) {
            startConky(item);
        }
    }
}

void ConkyManager::stopAllRunning()
{
    for (ConkyItem *item : m_conkyItems) {
        if (item->isRunning()) {
            stopConky(item);
        }
    }
}

void ConkyManager::saveSettings()
{
    m_settings.beginGroup("ConkyManager");
    m_settings.setValue("searchPaths", m_searchPaths);
    m_settings.setValue("startupDelay", m_startupDelay);

    m_settings.beginWriteArray("conkyItems");
    for (int i = 0; i < m_conkyItems.size(); ++i) {
        m_settings.setArrayIndex(i);
        QPointer<ConkyItem> item = m_conkyItems.at(i);
        if (!item) {
            continue;
        }
        m_settings.setValue("filePath", item->filePath());
        m_settings.setValue("enabled", item->isEnabled());
        m_settings.setValue("autostart", item->isAutostart());
    }
    m_settings.endArray();
    m_settings.endGroup();

    // Update startup script whenever settings are saved
    updateStartupScript();
}

void ConkyManager::loadSettings()
{
    m_settings.beginGroup("ConkyManager");

    m_searchPaths = m_settings
                        .value("searchPaths", QStringList() << QDir::homePath() + "/.conky"
                                                            << "/usr/share/mx-conky-data/themes")
                        .toStringList();

    m_startupDelay = m_settings.value("startupDelay", 20).toInt();

    int size = m_settings.beginReadArray("conkyItems");
    QHash<QString, ConkyItem *> settingsMap;

    for (int i = 0; i < size; ++i) {
        m_settings.setArrayIndex(i);
        QString filePath = m_settings.value("filePath").toString();
        if (!filePath.isEmpty() && QFile::exists(filePath)) {
            auto *item = new ConkyItem(filePath, this);
            item->setEnabled(m_settings.value("enabled", false).toBool());
            item->setAutostart(m_settings.value("autostart", false).toBool());
            settingsMap[filePath] = item;
        }
    }
    m_settings.endArray();
    m_settings.endGroup();

    scanForConkies();

    for (ConkyItem *item : m_conkyItems) {
        if (settingsMap.contains(item->filePath())) {
            ConkyItem *savedItem = settingsMap[item->filePath()];
            item->setEnabled(savedItem->isEnabled());
            item->setAutostart(savedItem->isAutostart());
            delete savedItem;
        }
    }
}

void ConkyManager::updateRunningStatus()
{
    // Skip if previous status check is still running
    if (m_statusCheckRunning || m_statusProcess->state() != QProcess::NotRunning) {
        return;
    }

    m_statusCheckRunning = true;

    // Use pattern matching to find conky processes (handles /usr/bin/conky, conky, etc.)
    m_statusProcess->start("pgrep", QStringList() << "conky");
}

void ConkyManager::onStatusProcessFinished()
{
    m_statusCheckRunning = false;

    if (m_statusProcess->exitCode() != 0 && m_statusProcess->exitCode() != 1) {
        // pgrep returns 1 when no processes found, which is normal
        // Only worry about other exit codes
        return;
    }

    QString output = m_statusProcess->readAllStandardOutput();
    QStringList pids = output.split('\n', Qt::SkipEmptyParts);

    // Build set of running config files by checking each PID
    QSet<QString> runningConfigs;
    for (const QString &pid : pids) {
        // Get command line for this PID
        QProcess cmdlineProcess;
        cmdlineProcess.start("ps", QStringList() << "-p" << pid << "-o"
                                                 << "command="
                                                 << "--no-headers");
        if (cmdlineProcess.waitForFinished(1000)) {
            QString cmdline = cmdlineProcess.readAllStandardOutput().trimmed();

            // Skip if this is not actually a conky process (could be grep, mx-conky, etc.)
            // Look for conky executable (conky, /usr/bin/conky, etc.) but not mx-conky application itself
            if (!cmdline.contains("conky") || cmdline.contains("./mx-conky") || cmdline.endsWith("mx-conky")) {
                continue;
            }

            // Find -c flag position in the command line
            int cFlagIndex = cmdline.indexOf(" -c ");
            if (cFlagIndex != -1) {
                // Extract everything after " -c " as the config path
                QString configPath = cmdline.mid(cFlagIndex + 4); // +4 for " -c "
                QString absolutePath = QFileInfo(configPath).absoluteFilePath();
                runningConfigs.insert(absolutePath);
            }
        }
    }

    // Check each item against the running configs
    for (ConkyItem *item : m_conkyItems) {
        bool wasRunning = item->isRunning();
        QString configFilePath = QFileInfo(item->filePath()).absoluteFilePath();
        bool isRunning = runningConfigs.contains(configFilePath);

        if (wasRunning != isRunning) {
            item->setRunning(isRunning);
            if (isRunning) {
                emit conkyStarted(item);
            } else {
                emit conkyStopped(item);
            }
        }
    }
}

void ConkyManager::onAutostartTimer()
{
    if (m_autostartQueue.isEmpty()) {
        return;
    }

    ConkyItem *item = m_autostartQueue.takeFirst();
    startConky(item);

    if (!m_autostartQueue.isEmpty()) {
        ConkyItem *nextItem = m_autostartQueue.first();
        int delay = nextItem->autostartDelay() * 1000;
        if (delay <= 0) {
            delay = 1000;
        }
        m_autostartTimer->start(delay);
    }
}

void ConkyManager::clearConkyItems()
{
    qDeleteAll(m_conkyItems);
    m_conkyItems.clear();
}

QString ConkyManager::getConkyProcess(const QString &configPath) const
{
    QString absolutePath = QFileInfo(configPath).absoluteFilePath();

    // Get all conky PIDs using pattern matching (handles /usr/bin/conky, conky, etc.)
    QProcess pgrepProcess;
    pgrepProcess.start("pgrep", QStringList() << "conky");
    pgrepProcess.waitForFinished(3000);

    QString pgrepOutput = pgrepProcess.readAllStandardOutput();
    QStringList pids = pgrepOutput.split('\n', Qt::SkipEmptyParts);

    // Check each PID to find the one with matching config path
    for (const QString &pid : pids) {
        QProcess cmdlineProcess;
        cmdlineProcess.start("ps", QStringList() << "-p" << pid << "-o" << "command=" << "--no-headers");
        if (cmdlineProcess.waitForFinished(1000)) {
            QString cmdline = cmdlineProcess.readAllStandardOutput().trimmed();

            // Skip if this is not actually a conky process (could be grep, mx-conky, etc.)
            // Look for conky executable (conky, /usr/bin/conky, etc.) but not mx-conky application itself
            if (!cmdline.contains("conky") || cmdline.contains("./mx-conky") || cmdline.endsWith("mx-conky")) {
                continue;
            }

            // Find -c flag position in the command line
            int cFlagIndex = cmdline.indexOf(" -c ");
            if (cFlagIndex != -1) {
                // Extract everything after " -c " as the config path
                QString configPath = cmdline.mid(cFlagIndex + 4); // +4 for " -c "
                QString foundAbsolutePath = QFileInfo(configPath).absoluteFilePath();

                // Check if this matches our target config path
                if (foundAbsolutePath == absolutePath) {
                    return pid;
                }
            }
        }
    }

    return QString(); // Not found
}

bool ConkyManager::isConkyRunning(const QString &configPath) const
{
    return !getConkyProcess(configPath).isEmpty();
}

void ConkyManager::scanDirectory(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    // Only get first-level subdirectories (no recursion)
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);

    for (const QString &subdir : subdirs) {
        scanConkyDirectory(dir.absoluteFilePath(subdir));
    }
}

void ConkyManager::scanConkyDirectory(const QString &path)
{
    static const QStringList skipExtensions
        = {".sh", ".png", ".jpg", ".jpeg", ".txt", ".bak", ".zip", ".ttf", ".md", ".py", ".lua", ".cmtheme"};
    static const QStringList skipNames = {"Changelog", "Notes", "README", "README!!", "OPTIONS"};

    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    dir.setFilter(QDir::Files | QDir::Readable);
    QFileInfoList allFiles = dir.entryInfoList();

    // Build a set of existing file paths for fast lookup
    QSet<QString> existingPaths;
    for (const ConkyItem *item : std::as_const(m_conkyItems)) {
        existingPaths.insert(item->filePath());
    }

    for (const QFileInfo &fileInfo : allFiles) {
        const QString &fileName = fileInfo.fileName();
        const QString &filePath = fileInfo.absoluteFilePath();

        if (fileName.startsWith('.')) {
            continue;
        }

        bool skip = false;
        for (const QString &ext : skipExtensions) {
            if (fileName.endsWith(ext, Qt::CaseInsensitive)) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }

        for (const QString &skipName : skipNames) {
            if (fileName.compare(skipName, Qt::CaseInsensitive) == 0) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }

        if (fileInfo.isDir()) {
            continue;
        }

        if (existingPaths.contains(filePath)) {
            continue;
        }

        m_conkyItems.append(new ConkyItem(filePath, this));
    }
}

void ConkyManager::updateStartupScript()
{
    QString home = QDir::homePath();
    QString conkyDir = home + "/.conky";
    QString startupScript = conkyDir + "/conky-startup.sh";

    // Ensure ~/.conky directory exists
    QDir dir;
    if (!dir.exists(conkyDir)) {
        dir.mkpath(conkyDir);
    }

    QString scriptContent;
    scriptContent += "#!/bin/sh\n\n";
    scriptContent += QString("sleep %1s\n").arg(m_startupDelay);
    scriptContent += "killall -u $(id -nu) conky 2>/dev/null\n";

    // Add conky start commands
    for (ConkyItem *item : m_conkyItems) {
        if (item->isAutostart()) {
            QString relativeDir = QFileInfo(item->filePath()).absolutePath().replace(home, "$HOME");
            QString relativePath = item->filePath().replace(home, "$HOME");

            scriptContent += QString("cd \"%1\"\n").arg(relativeDir);
            scriptContent += QString("conky -c \"%1\" &\n").arg(relativePath);
        }
    }

    scriptContent += "exit 0\n";

    // Write the script
    QFile file(startupScript);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << scriptContent;
        file.close();

        // Make script executable
        QFile::setPermissions(startupScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
                                                 | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    }
}

void ConkyManager::setAutostart(bool enabled)
{
    QString home = QDir::homePath();
    QString autostartDir = home + "/.config/autostart";
    QString desktopFile = autostartDir + "/conky.desktop";

    // Ensure autostart directory exists
    QDir dir;
    if (!dir.exists(autostartDir)) {
        dir.mkpath(autostartDir);
    }

    if (enabled) {
        QString conkyDir = home + "/.conky";
        QString startupScript = conkyDir + "/conky-startup.sh";

        QString desktopContent;
        desktopContent += "[Desktop Entry]\n";
        desktopContent += "Type=Application\n";
        desktopContent += QString("Exec=sh \"%1\"\n").arg(startupScript);
        desktopContent += "Hidden=false\n";
        desktopContent += "NoDisplay=false\n";
        desktopContent += "X-GNOME-Autostart-enabled=true\n";
        desktopContent += "Name=Conky\n";

        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << desktopContent;
            file.close();
        }
    } else {
        QFile::remove(desktopFile);
    }
}

bool ConkyManager::isAutostartEnabled() const
{
    QString home = QDir::homePath();
    QString desktopFile = home + "/.config/autostart/conky.desktop";
    return QFile::exists(desktopFile);
}

QString ConkyManager::copyFolderToUserConky(const QString &sourcePath)
{
    QFileInfo sourceInfo(sourcePath);
    QString folderName = sourceInfo.fileName();
    return copyFolderToUserConkyWithName(sourcePath, folderName);
}

QString ConkyManager::copyFolderToUserConkyWithName(const QString &sourcePath, const QString &newName)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isDir()) {
        qDebug() << "ConkyManager: Source path does not exist or is not a directory:" << sourcePath;
        return QString();
    }

    QString userConkyPath = QDir::homePath() + "/.conky";
    QString destPath = userConkyPath + "/" + newName;

    // Create ~/.conky directory if it doesn't exist
    QDir().mkpath(userConkyPath);

    // Remove existing destination if it exists
    if (QFileInfo::exists(destPath)) {
        QDir(destPath).removeRecursively();
    }

    // Copy the folder
    if (copyDirectoryRecursively(sourcePath, destPath)) {
        qDebug() << "ConkyManager: Successfully copied" << sourcePath << "to" << destPath;
        return destPath;
    } else {
        qDebug() << "ConkyManager: Failed to copy" << sourcePath << "to" << destPath;
        return QString();
    }
}

bool ConkyManager::copyDirectoryRecursively(const QString &sourceDir, const QString &destDir)
{
    QDir sourceDirectory(sourceDir);
    if (!sourceDirectory.exists()) {
        return false;
    }

    QDir destDirectory(destDir);
    if (!destDirectory.exists()) {
        destDirectory.mkpath(".");
    }

    // Copy all files
    QFileInfoList fileInfoList = sourceDirectory.entryInfoList(QDir::Files);
    for (const QFileInfo &fileInfo : fileInfoList) {
        QString srcFilePath = fileInfo.absoluteFilePath();
        QString destFilePath = destDir + "/" + fileInfo.fileName();

        if (!QFile::copy(srcFilePath, destFilePath)) {
            qDebug() << "ConkyManager: Failed to copy file" << srcFilePath << "to" << destFilePath;
            return false;
        }
    }

    // Copy all subdirectories recursively
    QFileInfoList subdirInfoList = sourceDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subdirInfo : subdirInfoList) {
        QString srcSubdirPath = subdirInfo.absoluteFilePath();
        QString destSubdirPath = destDir + "/" + subdirInfo.fileName();

        if (!copyDirectoryRecursively(srcSubdirPath, destSubdirPath)) {
            return false;
        }
    }

    return true;
}
