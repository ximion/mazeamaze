/*
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "encodewindow.h"
#include "ui_encodewindow.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QThread>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSvgWidget>
#include <QTimer>

#include "taskmanager.h"
#include "../videowriter.h"

EncodeWindow::EncodeWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::EncodeWindow)
{
    ui->setupUi(this);
    setWindowTitle("Syntalos - Video Encoding Queue");
    setWindowIcon(QIcon(":/icons/videorecorder.svg"));

    m_queueModel = new QueueModel(ui->tasksTable);

    ui->tasksTable->setModel(m_queueModel);
    ui->tasksTable->setItemDelegateForColumn(3, new ProgressBarDelegate(this));

    m_taskManager = new TaskManager(m_queueModel, this);
    QDBusConnection::sessionBus().registerObject("/", this);
    if (!QDBusConnection::sessionBus().registerService(EQUEUE_DBUS_SERVICE)) {
        fprintf(stderr, "%s\n",
                qPrintable(QDBusConnection::sessionBus().lastError().message()));
        exit(1);
    }

    // stretch out table columns
    for (int i = 0; i < ui->tasksTable->horizontalHeader()->count(); ++i) {
        if (i != 2)
            ui->tasksTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    ui->parallelTasksCountSpinBox->setMaximum(QThread::idealThreadCount() + 1);
    ui->parallelTasksCountSpinBox->setMinimum(1);
    ui->parallelTasksCountSpinBox->setValue(m_taskManager->parallelCount());
    connect(m_taskManager, &TaskManager::parallelCountChanged, [&](int count) {
        ui->parallelTasksCountSpinBox->setValue(count);
    });

    // enable the run button if new tasks are available
    connect(m_taskManager, &TaskManager::newTasksAvailable, [&]() {
        ui->runButton->setEnabled(true);
    });
    connect(m_taskManager, &TaskManager::encodingStarted, [&]() {
        ui->runButton->setEnabled(false);
    });
    ui->runButton->setEnabled(m_taskManager->tasksAvailable());

    // busy indicator
    m_busyIndicator = new QSvgWidget(ui->busyIndicatorContainer);
    m_busyIndicator->load(QStringLiteral(":/animations/busy.svg"));
    m_busyIndicator->setMaximumSize(QSize(40, 40));
    m_busyIndicator->setMinimumSize(QSize(40, 40));
    m_busyIndicator->hide();

    m_checkTimer = new QTimer;
    m_checkTimer->setInterval(1500);
    connect(m_checkTimer, &QTimer::timeout, [&]() {
        if (m_taskManager->isRunning())
            m_busyIndicator->show();
        else
            m_busyIndicator->hide();
    });
    m_checkTimer->start();

    // hide details display for now
    ui->detailsWidget->setVisible(false);
}

EncodeWindow::~EncodeWindow()
{
    delete ui;
}

void EncodeWindow::on_runButton_clicked()
{
    m_taskManager->processVideos();
}

void EncodeWindow::on_parallelTasksCountSpinBox_valueChanged(int value)
{
    m_taskManager->setParallelCount(value);
}

void EncodeWindow::closeEvent(QCloseEvent *event)
{
    if (m_taskManager->allTasksCompleted()) {
        event->accept();
        QApplication::quit();
    } else {
        QMessageBox::warning(this, QStringLiteral("Encoding in progress"),
                             QStringLiteral("You can not close this tool while there are still encoding tasks ongoing or pending.\n"
                                            "Please encode all videos before quitting."));
        event->ignore();
    }
}