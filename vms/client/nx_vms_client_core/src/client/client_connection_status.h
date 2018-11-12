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

class QnClientConnectionStatus: public QObject
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
    void warn(const QString &message) const;

private:
    QnConnectionState m_state;
    QMultiMap<QnConnectionState, QnConnectionState> m_allowedTransactions;
};

