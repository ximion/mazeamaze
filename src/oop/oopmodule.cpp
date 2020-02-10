/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "oopmodule.h"

#include <QEventLoop>
#include <QRemoteObjectNode>

#include "oopworkerconnector.h"

class OOPModuleRunData
{
public:
    OOPModuleRunData()
    {
        repNode.reset(new QRemoteObjectNode);
    }
    ~OOPModuleRunData()
    {}

    std::unique_ptr<QRemoteObjectNode> repNode;
    QSharedPointer<OOPWorkerReplica> replica;
    QSharedPointer<OOPWorkerConnector> wc;
};

#pragma GCC diagnostic ignored "-Wpadded"
class OOPModule::Private
{
public:
    Private()
        : runData(new OOPModuleRunData)
    {}
    ~Private() {}

    QString pyScript;
    QString pyEnv;

    QSharedPointer<OOPModuleRunData> runData;
};
#pragma GCC diagnostic pop

OOPModule::OOPModule(QObject *parent)
    : AbstractModule(parent),
      d(new OOPModule::Private)
{
}

OOPModule::~OOPModule()
{
}

ModuleFeatures OOPModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY |
           ModuleFeature::SHOW_SETTINGS;
}

bool OOPModule::prepare(const QString &storageRootDir, const TestSubject &testSubject)
{
    Q_UNUSED(storageRootDir)
    Q_UNUSED(testSubject)

    return true;
}

bool OOPModule::oopPrepare(QEventLoop *loop)
{
    // We have to do all the setup stuff on thread creation, as moving QObject
    // instances between different threads is not ideal and the QRO connection
    // occasionally doesn't get established properly if we shift work between threads
    d->runData.reset(new OOPModuleRunData);

    d->runData->replica.reset(d->runData->repNode->acquire<OOPWorkerReplica>());
    d->runData->wc.reset(new OOPWorkerConnector(d->runData->replica));

    auto wc = d->runData->wc;

    // connect some of the important signals of our replica
    connect(d->runData->replica.data(), &OOPWorkerReplica::error, this, [&](const QString &message) {
        raiseError(message);
        m_running = false;
    });
    connect(d->runData->replica.data(), &OOPWorkerReplica::statusMessage, this, [&](const QString &text) {
        setStatusMessage(text);
    });

    if (!wc->connectAndRun()) {
        raiseError("Unable to start worker process!");
        return false;
    }

    wc->setInputPorts(inPorts());
    wc->setOutputPorts(outPorts());

    wc->initWithPythonScript(d->pyScript, d->pyEnv);

    // check if we already received messages from the worker,
    // such as errors or output port metadata updates
    loop->processEvents();

    // set all outgoing streams as active (which propagates metadata)
    for (auto &port : outPorts())
        port->streamVar()->start();

    if (wc->failed())
        return false;

    setStateReady();
    return true;
}

void OOPModule::oopStart(QEventLoop *)
{
    d->runData->wc->start(m_timer->startTime());
}

void OOPModule::oopRunEvent(QEventLoop *loop)
{
    // first thing to do: Look for possible (error) signals from our worker
    loop->processEvents();

    // forward incoming data to the worker
    d->runData->wc->forwardInputData(loop);

    if (d->runData->wc->failed())
        m_running = false;
}

void OOPModule::oopFinalize(QEventLoop *loop)
{
    d->runData->wc->terminate(loop);
    d->runData.reset();
}

void OOPModule::loadPythonScript(const QString &script, const QString &env)
{
    d->pyScript = script;
    d->pyEnv = env;
}

void OOPModule::loadPythonFile(const QString &fname, const QString &env)
{
    QFile f(fname);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        raiseError(QStringLiteral("Unable to open Python script file: %1").arg(fname));
        return;
    }
    QTextStream in(&f);
    loadPythonScript(in.readAll(), env);
}