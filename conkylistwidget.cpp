/**********************************************************************
 *  ConkyWidget.cpp
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

#include "conkylistwidget.h"
#include <QDebug>
#include <QFileInfo>
#include <QHeaderView>
#include <QPixmap>
#include <QTimer>

ConkyItemWidget::ConkyItemWidget(ConkyItem *item, QWidget *parent)
    : QWidget(parent),
      m_item(item)
{
    setupUI();
    updateFromItem();

    connect(item, &ConkyItem::dataChanged, this, &ConkyItemWidget::updateFromItem);
}

ConkyItem *ConkyItemWidget::conkyItem() const
{
    return m_item;
}

void ConkyItemWidget::updateFromItem()
{
    if (!m_item) {
        return;
    }

    m_nameLabel->setText(m_item->name());
    m_pathLabel->setText(m_item->filePath());

    m_enabledCheckBox->blockSignals(true);
    m_enabledCheckBox->setChecked(m_item->isEnabled());
    m_enabledCheckBox->blockSignals(false);

    updateRunToggleButton();

    QString status = m_item->isRunning() ? tr("Running") : tr("Stopped");
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(m_item->isRunning() ? "color: green;" : "color: red;");
}

void ConkyItemWidget::onEnabledChanged(bool enabled)
{
    emit enabledChanged(m_item, enabled);
}

void ConkyItemWidget::onEditClicked()
{
    emit editRequested(m_item);
}

void ConkyItemWidget::onCustomizeClicked()
{
    emit customizeRequested(m_item);
}

void ConkyItemWidget::onRunToggleClicked()
{
    emit runToggleRequested(m_item);
}

void ConkyItemWidget::onDeleteClicked()
{
    emit deleteRequested(m_item);
}

void ConkyItemWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);

    auto *topLayout = new QHBoxLayout;

    m_nameLabel = new QLabel;
    m_nameLabel->setStyleSheet("font-weight: bold;");

    m_statusLabel = new QLabel;
    m_statusLabel->setAlignment(Qt::AlignRight);

    topLayout->addWidget(m_nameLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_statusLabel);

    m_pathLabel = new QLabel;
    m_pathLabel->setStyleSheet("color: gray; font-size: 10px;");

    auto *controlsLayout = new QHBoxLayout;

    m_enabledCheckBox = new QCheckBox(tr("Autostart"));

    m_editButton = new QPushButton(tr("Edit"));
    m_editButton->setIcon(QIcon::fromTheme("edit"));

    m_customizeButton = new QPushButton(tr("Customize"));
    m_customizeButton->setIcon(QIcon::fromTheme("preferences-desktop"));

    m_runToggleButton = new QPushButton;

    m_deleteButton = new QPushButton(tr("Delete"));
    m_deleteButton->setIcon(QIcon::fromTheme("edit-delete"));
    m_deleteButton->setStyleSheet("QPushButton { color: red; }");

    controlsLayout->addWidget(m_enabledCheckBox);
    controlsLayout->addStretch();
    controlsLayout->addWidget(m_deleteButton);
    controlsLayout->addWidget(m_editButton);
    controlsLayout->addWidget(m_customizeButton);
    controlsLayout->addWidget(m_runToggleButton);

    layout->addLayout(topLayout);
    layout->addWidget(m_pathLabel);
    layout->addLayout(controlsLayout);

    connect(m_enabledCheckBox, &QCheckBox::toggled, this, &ConkyItemWidget::onEnabledChanged);
    connect(m_editButton, &QPushButton::clicked, this, &ConkyItemWidget::onEditClicked);
    connect(m_customizeButton, &QPushButton::clicked, this, &ConkyItemWidget::onCustomizeClicked);
    connect(m_runToggleButton, &QPushButton::clicked, this, &ConkyItemWidget::onRunToggleClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &ConkyItemWidget::onDeleteClicked);
}

void ConkyItemWidget::updateRunToggleButton()
{
    if (!m_item) {
        return;
    }

    if (m_item->isRunning()) {
        m_runToggleButton->setText(tr("Stop"));
        m_runToggleButton->setIcon(QIcon::fromTheme("stop"));
    } else {
        m_runToggleButton->setText(tr("Start"));
        m_runToggleButton->setIcon(QIcon::fromTheme("start"));
    }
}

ConkyListWidget::ConkyListWidget(ConkyManager *manager, QWidget *parent)
    : QWidget(parent),
      m_manager(manager)
{
    setupUI();

    connect(m_manager, &ConkyManager::conkyItemsChanged, this, &ConkyListWidget::onConkyItemsChanged);
    connect(m_manager, &ConkyManager::conkyStarted, this, [this](ConkyItem *) {
        // Items will update themselves via dataChanged signal
    });
    connect(m_manager, &ConkyManager::conkyStopped, this, [this](ConkyItem *) {
        // Items will update themselves via dataChanged signal
    });

    refreshList();
}

void ConkyListWidget::refreshList()
{
    onConkyItemsChanged();
}

ConkyItem *ConkyListWidget::selectedConkyItem() const
{
    QTreeWidgetItem *currentItem = m_treeWidget->currentItem();
    if (!currentItem) {
        return nullptr;
    }

    for (auto it = m_treeItems.begin(); it != m_treeItems.end(); ++it) {
        if (it.value() == currentItem) {
            return it.key();
        }
    }

    return nullptr;
}

void ConkyListWidget::selectConkyItem(const QString &filePath)
{
    // Find the ConkyItem with the given file path
    ConkyItem *targetItem = nullptr;
    for (ConkyItem *item : m_manager->conkyItems()) {
        if (item->filePath() == filePath) {
            targetItem = item;
            break;
        }
    }

    if (!targetItem) {
        return;
    }

    // Find the corresponding tree widget item
    if (m_treeItems.contains(targetItem)) {
        QTreeWidgetItem *treeItem = m_treeItems[targetItem];
        if (treeItem) {
            m_treeWidget->setCurrentItem(treeItem);
            m_treeWidget->scrollToItem(treeItem);
        }
    }
}

void ConkyListWidget::onConkyItemsChanged()
{
    m_treeWidget->clear();
    m_itemWidgets.clear();
    m_treeItems.clear();

    QList<ConkyItem *> items = m_manager->conkyItems();
    for (ConkyItem *item : items) {
        addConkyItem(item);
    }

    // Auto-select first item only on initial load
    if (!m_hasAutoSelected && m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem *firstItem = m_treeWidget->topLevelItem(0);
        m_treeWidget->setCurrentItem(firstItem);
        m_hasAutoSelected = true;

        // Use a short timer to ensure the tree widget is fully initialized
        QTimer::singleShot(50, this, [this]() { onItemSelectionChanged(); });
    }
}

void ConkyListWidget::onItemSelectionChanged()
{
    ConkyItem *item = selectedConkyItem();
    emit itemSelectionChanged(item);
}

void ConkyListWidget::onEnabledChanged(ConkyItem *item, bool enabled)
{
    item->setEnabled(enabled);
    item->setAutostart(enabled); // Enable checkbox now controls autostart
    m_manager->saveSettings();
}

void ConkyListWidget::onEditRequested(ConkyItem *item)
{
    emit editRequested(item);
}

void ConkyListWidget::onCustomizeRequested(ConkyItem *item)
{
    emit customizeRequested(item);
}

void ConkyListWidget::onRunToggleRequested(ConkyItem *item)
{
    if (item->isRunning()) {
        m_manager->stopConky(item);
    } else {
        m_manager->startConky(item);
    }
}

void ConkyListWidget::onDeleteRequested(ConkyItem *item)
{
    emit deleteRequested(item);
}

void ConkyListWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_treeWidget = new QTreeWidget;
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(false);
    m_treeWidget->setAlternatingRowColors(true);

    connect(m_treeWidget, &QTreeWidget::currentItemChanged, this, &ConkyListWidget::onItemSelectionChanged);

    layout->addWidget(m_treeWidget);
}

void ConkyListWidget::addConkyItem(ConkyItem *item)
{
    auto *treeItem = new QTreeWidgetItem(m_treeWidget);
    auto *itemWidget = new ConkyItemWidget(item);

    connect(itemWidget, &ConkyItemWidget::enabledChanged, this, &ConkyListWidget::onEnabledChanged);
    connect(itemWidget, &ConkyItemWidget::editRequested, this, &ConkyListWidget::onEditRequested);
    connect(itemWidget, &ConkyItemWidget::customizeRequested, this, &ConkyListWidget::onCustomizeRequested);
    connect(itemWidget, &ConkyItemWidget::runToggleRequested, this, &ConkyListWidget::onRunToggleRequested);
    connect(itemWidget, &ConkyItemWidget::deleteRequested, this, &ConkyListWidget::onDeleteRequested);

    m_treeWidget->setItemWidget(treeItem, 0, itemWidget);

    m_itemWidgets[item] = itemWidget;
    m_treeItems[item] = treeItem;
}

void ConkyListWidget::removeConkyItem(ConkyItem *item)
{
    if (m_treeItems.contains(item)) {
        delete m_treeItems[item];
        m_treeItems.remove(item);
    }

    if (m_itemWidgets.contains(item)) {
        delete m_itemWidgets[item];
        m_itemWidgets.remove(item);
    }
}

void ConkyListWidget::setStatusFilter(const QString &filter)
{
    m_statusFilter = filter;
    applyFilters();
}

void ConkyListWidget::setSearchText(const QString &searchText)
{
    m_searchText = searchText;
    applyFilters();
}

void ConkyListWidget::applyFilters()
{
    for (auto it = m_treeItems.begin(); it != m_treeItems.end(); ++it) {
        ConkyItem *item = it.key();
        QTreeWidgetItem *treeItem = it.value();

        bool visible = itemMatchesFilters(item);
        treeItem->setHidden(!visible);
    }
}

bool ConkyListWidget::itemMatchesFilters(ConkyItem *item) const
{
    if (!item) {
        return false;
    }

    // Apply status filter
    if (m_statusFilter == "Running" && !item->isRunning()) {
        return false;
    }
    if (m_statusFilter == "Stopped" && item->isRunning()) {
        return false;
    }

    // Apply search text filter
    if (!m_searchText.isEmpty()) {
        QString itemName = item->name().toLower();
        QString folderName = QFileInfo(item->directory()).fileName().toLower();
        QString searchLower = m_searchText.toLower();
        if (!itemName.contains(searchLower) && !folderName.contains(searchLower)) {
            return false;
        }
    }

    return true;
}

ConkyPreviewWidget::ConkyPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ConkyPreviewWidget::setConkyItem(ConkyItem *item)
{
    m_currentItem = item;
    updatePreview();
}

void ConkyPreviewWidget::clearPreview()
{
    m_currentItem = nullptr;
    updatePreview();
}

void ConkyPreviewWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    m_nameLabel = new QLabel;
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_nameLabel->setAlignment(Qt::AlignCenter);

    m_pathLabel = new QLabel;
    m_pathLabel->setStyleSheet("color: gray; font-size: 10px;");
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setWordWrap(true);

    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(200, 150);
    m_previewLabel->setStyleSheet("border: 1px solid gray; background-color: white;");

    m_descriptionLabel = new QLabel;
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignTop);

    layout->addWidget(m_nameLabel);
    layout->addWidget(m_pathLabel);
    layout->addWidget(m_previewLabel, 1);
    layout->addWidget(m_descriptionLabel);
}

void ConkyPreviewWidget::updatePreview()
{
    if (!m_currentItem) {
        m_nameLabel->setText(tr("No Conky Selected"));
        m_pathLabel->clear();
        m_previewLabel->setText(tr("Select a conky from the list to see its preview"));
        m_descriptionLabel->clear();
        return;
    }

    m_nameLabel->setText(m_currentItem->name());
    m_pathLabel->setText(m_currentItem->filePath());
    m_descriptionLabel->setText(m_currentItem->description());

    QString previewPath = m_currentItem->previewImage();
    if (!previewPath.isEmpty() && QFile::exists(previewPath)) {
        QPixmap pixmap(previewPath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_previewLabel->setPixmap(scaledPixmap);

            // Emit signal with original image size
            emit previewImageLoaded(pixmap.size());
        } else {
            m_previewLabel->setText(tr("Preview image could not be loaded"));
        }
    } else {
        m_previewLabel->setText(tr("No preview image available"));
    }
}
