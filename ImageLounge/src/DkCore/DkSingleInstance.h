/*******************************************************************************************************
 DkSingleInstance.h
 Created on: 10.10.2026

 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances

 Copyright (C) 2026 nomacs authors

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#pragma once

#include <QObject>
#include <QStringList>

#include "nmc_config.h"

class QLocalServer;
class QWidget;

namespace nmc
{

/**
 * @brief Implements the "open in existing instance" feature.
 *
 * When multiple instances are disabled, a new nomacs process tries to
 * deliver its command line files to a running instance via a local
 * socket. The running instance listens on the socket and loads the
 * files it receives.
 */
class DllCoreExport DkSingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit DkSingleInstance(QObject *parent = nullptr);
    ~DkSingleInstance() override;

    /// @brief starts the server; returns false if the server cannot listen
    bool startServer();

    /**
     * @brief sends @p files to a running nomacs instance (if any) and raises it.
     * @return true if the files were delivered (caller should exit).
     */
    static bool activateExisting(const QStringList &files);

    /// @brief unique (per user) socket/pipe name used for the IPC
    static QString serverName();

    /// @brief brings @p window to the foreground, also when Windows denies focus stealing;
    ///        if @p maximize is true, the window is also maximized
    static void activateWindow(QWidget *window, bool maximize = false);

signals:
    /// @brief emitted on the running instance when files are requested
    void filesRequested(const QStringList &files);

private slots:
    void onNewConnection();

private:
    QLocalServer *mServer = nullptr;
};

}
