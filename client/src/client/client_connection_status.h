#ifndef QN_CLIENT_CONNECTION_STATUS_H
#define QN_CLIENT_CONNECTION_STATUS_H

enum class QnConnectionState {
    Invalid,
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Ready

};

class QnClientConnectionStatus {
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

#endif // QN_CLIENT_CONNECTION_STATUS_H
