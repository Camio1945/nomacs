/*******************************************************************************************************
 DkSingleInstance.cpp
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

#include "DkSingleInstance.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QWidget>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace nmc
{

DkSingleInstance::DkSingleInstance(QObject *parent) : QObject(parent)
{
}

DkSingleInstance::~DkSingleInstance()
{
    if (mServer)
        mServer->close();
}

QString DkSingleInstance::serverName()
{
    // scope the name per user (on unix the socket lives in a shared tmp dir)
    const QString user = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0);
    const QString suffix = QString::fromUtf8(
        QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
    return "nomacs-image-lounge-" + suffix;
}

void DkSingleInstance::activateWindow(QWidget *window, bool maximize)
{
#ifdef Q_OS_WIN
    // Windows denies foreground activation for background processes - the
    // request is rejected and only the taskbar icon flashes. Attaching our
    // thread to the foreground thread's input queue grants us permission.
    HWND hwnd = reinterpret_cast<HWND>(window->winId());

    ShowWindow(hwnd, maximize ? SW_MAXIMIZE : SW_RESTORE); // un-minimize/resize

    const HWND foreWnd = GetForegroundWindow();
    const DWORD foreThread = GetWindowThreadProcessId(foreWnd, nullptr);
    const DWORD thisThread = GetCurrentThreadId();
    const bool attached = foreThread != thisThread && AttachThreadInput(thisThread, foreThread, TRUE);

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);

    if (attached)
        AttachThreadInput(thisThread, foreThread, FALSE);
#else
    if (maximize)
        window->showMaximized();
    else
        window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    window->raise();
    window->activateWindow();
#endif
}

bool DkSingleInstance::startServer()
{
    if (mServer && mServer->isListening())
        return true;

    // remove a stale socket (e.g. left over from a crashed instance on unix)
    QLocalServer::removeServer(serverName());

    mServer = new QLocalServer(this);

    if (!mServer->listen(serverName())) {
        qWarning() << "single instance server failed to listen:" << mServer->errorString();
        delete mServer;
        mServer = nullptr;
        return false;
    }

    connect(mServer, &QLocalServer::newConnection, this, &DkSingleInstance::onNewConnection);
    return true;
}

void DkSingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = mServer->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);

        // wait for the client to finish writing before reading
        if (socket->waitForReadyRead(1000)) {
            const QByteArray data = socket->readAll();
            const QStringList files = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
            emit filesRequested(files);
        }
        socket->disconnectFromServer();
    }
}

bool DkSingleInstance::activateExisting(const QStringList &files)
{
    QLocalSocket socket;

    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500)) {
        qInfo() << "no running nomacs instance found - starting a new one";
        return false;
    }

    socket.write(files.join('\n').toUtf8());
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();

    qInfo() << "forwarded" << files.size() << "file(s) to the running nomacs instance";
    return true;
}

}
