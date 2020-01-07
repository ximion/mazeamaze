/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef OPTIONALWAITCONDITION_H
#define OPTIONALWAITCONDITION_H

#include <QSharedPointer>

class AbstractModule;

/**
 * @brief The OptionalWaitCondition class
 *
 * Create a thread barrier to synchronize a
 * set of threads to run at once.
 */
class OptionalWaitCondition {
    friend class Engine;
public:
    OptionalWaitCondition();

    void wait();
    void wait(AbstractModule *mod);

    uint waitingCount() const;

private:
    class OWCData;
    QSharedPointer<OWCData> d;
    Q_DISABLE_COPY(OptionalWaitCondition)

    void wakeAll();
    void reset();
};

#endif // OPTIONALWAITCONDITION_H
