#pragma once

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

class QnClientConnectionStatus
{
public:
    QnClientConnectionStatus(QnConnectionState state = QnConnectionState::Disconnected);

    QnConnectionState state() const;
    void setState(QnConnectionState state);

private:
    void warn(const QString &message) const;

private:
    QnConnectionState m_state;
    QMultiMap<QnConnectionState, QnConnectionState> m_allowedTransactions;
};

