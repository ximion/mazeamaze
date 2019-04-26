/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef ABSTRACTMODULE_H
#define ABSTRACTMODULE_H

#include <QObject>
#include <chrono>

/**
 * @brief The ModuleState enum
 *
 * Describes the state a module can be in. The state is usually displayed
 * to the user via a module indicator widget.
 */
enum class ModuleState {
    UNKNOWN,
    PREPARING,
    READY,
    RUNNING,
    ERROR
};

namespace cv {
class Mat;
}

class AbstractModule : public QObject
{
    Q_OBJECT
public:
    explicit AbstractModule(QObject *parent = nullptr);

    virtual ModuleState state() const;

    /**
     * @brief Initialize the module
     *
     * Initialize this module. This method is called once after construction.
     * @return true if success
     */
    virtual bool initialize() = 0;

    /**
     * @brief Prepare for an experiment run
     *
     * Prepare this module to run. This method is called once
     * prior to every experiment run.
     * @return true if success
     */
    virtual bool prepare() = 0;

    /**
     * @brief Execute tasks once per processing loop
     *
     * Run one iteration for this module. This function is called in a loop,
     * so make sure it never blocks.
     * @return true if no error
     */
    virtual bool runCycle();

    /**
     * @brief Execute this module's threads.
     * This method is run once when the experiment is started.
     * @return true if successful.
     */
    virtual bool runThreads();

    /**
     * @brief Stop running an experiment.
     * Stop execution of an experiment. This method is called after
     * prepare() was run.
     */
    virtual void stop() = 0;

    /**
     * @brief Finalize this module.
     * This method is called before the module itself is destroyed.
     */
    virtual void finalize();

    /**
     * @brief Show the display widgets of this module
     */
    virtual void showDisplayUi();

    /**
     * @brief Show the configuration UI of thos module
     */
    virtual void showSettingsUi();

    /**
     * @brief Return last error
     * @return The last error message generated by this module
     */
    QString lastError() const;

public slots:
    virtual void receiveFrame(const cv::Mat& frame, const std::chrono::milliseconds& timestamp);

signals:
    void stateChanged(ModuleState state);
    void errorMessage(const QString& message);

protected:
    void setState(ModuleState state);
    void setLastError(const QString& message);

    QString m_storageDir;

private:
    ModuleState m_state;
    QString m_lastError;
};

#endif // ABSTRACTMODULE_H
