// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>

enum class QnConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Ready
};

class QnConnectionStateUtils
{
public:
    static QString toString(QnConnectionState state);
};

class NX_VMS_CLIENT_CORE_API QnClientConnectionStatus: public QObject
{
    Q_OBJECT
public:
    QnClientConnectionStatus(QnConnectionState state = QnConnectionState::Disconnected,
        QObject* parent = nullptr);

    QnConnectionState state() const;
    void setState(QnConnectionState state);

    bool operator!=(QnConnectionState state) const;
    bool operator==(QnConnectionState state) const;
    QnClientConnectionStatus& operator=(QnConnectionState state);

signals:
    void stateChanged(QnConnectionState value);

private:
    QnConnectionState m_state;
    QMultiMap<QnConnectionState, QnConnectionState> m_allowedTransactions;
};
