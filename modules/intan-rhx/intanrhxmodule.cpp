/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "intanrhxmodule.h"

#include <QTimer>

#include "boardselectdialog.h"
#include "chanexportdialog.h"

SYNTALOS_MODULE(IntanRhxModule)


QString IntanRhxModuleInfo::id() const
{
    return QStringLiteral("intan-rhx");
}

QString IntanRhxModuleInfo::name() const
{
    return QStringLiteral("Intan RHX");
}

QString IntanRhxModuleInfo::description() const
{
    return QStringLiteral("Record electrophysiological signals from any Intan RHD or RHS system using "
                          "an RHD USB interface board, RHD recording controller, or RHS stim/recording controller.");
}

QString IntanRhxModuleInfo::license() const
{
    return QStringLiteral("Copyright (c) 2020-2021 <a href=\"https://intantech.com/\">Intan Technologies</a> [GPLv3+]");
}

QIcon IntanRhxModuleInfo::icon() const
{
    return QIcon(":/module/intan-rhx");
}

bool IntanRhxModuleInfo::singleton() const
{
    return true;
}

AbstractModule *IntanRhxModuleInfo::createModule(QObject *parent)
{
    return new IntanRhxModule(parent);
}

IntanRhxModule::IntanRhxModule(QObject *parent)
    : AbstractModule(parent),
      m_chanExportDlg(nullptr),
      m_sysState(nullptr)
{
    m_boardSelectDlg = new BoardSelectDialog;
    m_ctlWindow = m_boardSelectDlg->getControlWindow();
    if (m_ctlWindow != nullptr)
        m_ctlWindow->hide();
    m_sysState = m_boardSelectDlg->systemState();
    m_controllerIntf = m_boardSelectDlg->getControllerInterface();
    m_controllerIntf->setSyntalosModule(this);

    m_boardSelectDlg->setWindowIcon(QIcon(":/module/intan-rhx"));
    m_ctlWindow->setWindowIcon(QIcon(":/module/intan-rhx"));

    if (m_sysState == nullptr)
        return;
}

IntanRhxModule::~IntanRhxModule()
{
    delete m_boardSelectDlg;
    delete m_ctlWindow;
}

bool IntanRhxModule::initialize()
{
    if (m_ctlWindow == nullptr) {
        raiseError(QStringLiteral("No reference to control window found. This is an internal error."));
        return false;
    }
    if (m_sysState == nullptr) {
        raiseError(QStringLiteral("Failed to initialize module."));
        return false;
    }

    m_chanExportDlg = new ChanExportDialog(m_sysState);
    addSettingsWindow(m_chanExportDlg);
    addDisplayWindow(m_ctlWindow, false);

    connect(m_chanExportDlg, &ChanExportDialog::exportedChannelsChanged,
            this, &IntanRhxModule::onExportedChannelsChanged);

    m_controllerIntf->setSyntalosStartTime(symaster_clock::now());
    return true;
}

ModuleFeatures IntanRhxModule::features() const
{
    return ModuleFeature::CALL_UI_EVENTS |
           ModuleFeature::SHOW_SETTINGS |
            ModuleFeature::SHOW_DISPLAY;
}

ModuleDriverKind IntanRhxModule::driver() const
{
    return ModuleDriverKind::NONE;
}

void IntanRhxModule::updateStartWaitCondition(OptionalWaitCondition *waitCondition)
{
    m_controllerIntf->updateStartWaitCondition(waitCondition);
}

bool IntanRhxModule::prepare(const TestSubject &)
{
    // the Intan module is a singleton, so we can "grab" this very generic name here
    auto dstore = getOrCreateDefaultDataset(QStringLiteral("intan-signals"));

    // we use the ugly scanning method -for now
    dstore->setDataScanPattern(QStringLiteral("*.rhd"));
    dstore->setAuxDataScanPattern(QStringLiteral("*.tsync"));

    const auto intanBasePart = QStringLiteral("%1_data.rhd").arg(dstore->collectionId().toString(QUuid::WithoutBraces).left(4));
    const auto intanBaseFilename = dstore->pathForDataBasename(intanBasePart);
    if (intanBaseFilename.isEmpty())
        return false;
    m_ctlWindow->setSaveFilenameTemplate(intanBaseFilename);

    // set port metadata
    const auto sampleRate = m_controllerIntf->getRhxController()->getSampleRate();
    for (auto &blocks : intSdiByGroupChannel) {
        for (uint i = 0; i < blocks.size(); ++ i) {
            auto &sdi = blocks[i];
            if (!sdi.active)
                continue;
            sdi.stream->setMetadataValue(QStringLiteral("samplingrate"), sampleRate);
            sdi.stream->setMetadataValue(QStringLiteral("channel_index_first"), i);
            sdi.stream->setMetadataValue(QStringLiteral("channel_index_last"),  i);
        }
    }
    for (auto &blocks : floatSdiByGroupChannel) {
        for (uint i = 0; i < blocks.size(); ++ i) {
            auto &sdi = blocks[i];
            if (!sdi.active)
                continue;
            sdi.stream->setMetadataValue(QStringLiteral("samplingrate"), sampleRate);
            sdi.stream->setMetadataValue(QStringLiteral("channel_index_first"), i);
            sdi.stream->setMetadataValue(QStringLiteral("channel_index_last"),  i);
        }
    }

    // start output port streams
    for (auto &port : outPorts())
        port->startStream();

    // set up slave-clock synchronizer
    clockSync = initCounterSynchronizer(sampleRate);
    clockSync->setStrategies(TimeSyncStrategy::WRITE_TSYNCFILE);
    clockSync->setTimeSyncBasename(intanBaseFilename, dstore->collectionId());

    // permit 1.5ms tolerance - this was a very realistic tolerance to achieve in tests,
    // while lower values resulted in constant adjustment attempts
    clockSync->setTolerance(std::chrono::microseconds(1500));

    // we only permit calibration with the very first data block - this seems to be sufficient and
    // yielded the best results (due to device and USB buffering, the later data blocks are more
    // susceptible to error)
  //!!!  clockSync->setCalibrationBlocksCount((m_intanUi->getSampleRate() / Rhd2000DataBlock::getSamplesPerDataBlock()) * 10);

    if (!clockSync->start()) {
        raiseError(QStringLiteral("Unable to set up timestamp synchronizer!"));
        return false;
    }

    // run (but wait for the starting signal)
    m_ctlWindow->recordControllerSlot();
    return true;
}

void IntanRhxModule::processUiEvents()
{
    m_controllerIntf->controllerRunIter();
}

void IntanRhxModule::start()
{
    m_controllerIntf->setSyntalosStartTime(m_syTimer->startTime());
    AbstractModule::start();
}

void IntanRhxModule::stop()
{
    m_ctlWindow->stopControllerSlot();
    safeStopSynchronizer(clockSync);

    AbstractModule::stop();
}

void IntanRhxModule::setPortSignalBlockSampleSize(size_t sampleNum)
{
    for (auto &blocks : intSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            sdi.signalBlock->timestamps.resize(sampleNum);
            sdi.signalBlock->data.resize(sampleNum, 1);
        }
    }

    for (auto &blocks : floatSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            sdi.signalBlock->timestamps.resize(sampleNum);
            sdi.signalBlock->data.resize(sampleNum, 1);
        }
    }
}

void IntanRhxModule::onExportedChannelsChanged(const QList<Channel *> &channels)
{
    // reset all our ports, we are adding new ones
    clearOutPorts();
    clearInPorts();
    intSdiByGroupChannel.clear();
    floatSdiByGroupChannel.clear();

    auto signalSources = m_sysState->signalSources;

    // add new ports
    for (const auto &channel : channels) {
        bool isDigital = (channel->getSignalType() == BoardDigitalInSignal) ||
                         (channel->getSignalType() == BoardDigitalOutSignal);
        if (isDigital) {
            const auto groupIndex = signalSources->groupIndexByName(channel->getGroupName());
            if ((int) intSdiByGroupChannel.size() <= groupIndex)
                intSdiByGroupChannel.resize(groupIndex + 1);
            if ((int) intSdiByGroupChannel[groupIndex].size() <= channel->getNativeChannelNumber())
                intSdiByGroupChannel[groupIndex].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<IntSignalBlock> sdi(groupIndex, channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<IntSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());
            sdi.active = true;

            intSdiByGroupChannel[groupIndex][channel->getNativeChannelNumber()] = sdi;
        } else {
            const auto groupIndex = signalSources->groupIndexByName(channel->getGroupName());
            if ((int) floatSdiByGroupChannel.size() <= groupIndex)
                floatSdiByGroupChannel.resize(groupIndex + 1);
            if ((int) floatSdiByGroupChannel[groupIndex].size() <= channel->getNativeChannelNumber())
                floatSdiByGroupChannel[groupIndex].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<FloatSignalBlock> sdi(groupIndex, channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<FloatSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());
            sdi.active = true;

            floatSdiByGroupChannel[groupIndex][channel->getNativeChannelNumber()] = sdi;
        }
    }
}
