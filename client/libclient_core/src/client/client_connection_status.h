#pragma once

enum class QnConnectionState
{
    Invalid,
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

class QnClientConnectionStatus
{
public:
    QnClientConnectionStatus();

    QnConnectionState state() const;
    void setState(QnConnectionState state);

private:
    void warn(const QString &message) const;

private:
    QnConnectionState m_state;
    QMultiMap<QnConnectionState, QnConnectionState> m_allowedTransactions;
};

