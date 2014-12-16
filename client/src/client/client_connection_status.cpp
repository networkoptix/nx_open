#include "client_connection_status.h"

//#define STRICT_STATE_CONTROL

namespace {

    QString stateToString(QnConnectionState state) {
        switch (state) {
        case QnConnectionState::Invalid:                return lit("Invalid");
        case QnConnectionState::Disconnected:           return lit("Disconnected");
        case QnConnectionState::Connecting:             return lit("Connecting");
        case QnConnectionState::Connected:              return lit("Connected");
        case QnConnectionState::Reconnecting:           return lit("Reconnecting");
        case QnConnectionState::Ready:                  return lit("Ready");
        }
        qWarning() << "invalid state" << QString::number(static_cast<int>(state));
        return QString();
    }

}

QnClientConnectionStatus::QnClientConnectionStatus():
    m_state(QnConnectionState::Invalid)
{
    /* Default way. */
    m_allowedTransactions.insert(QnConnectionState::Invalid,        QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Disconnected,   QnConnectionState::Connecting);
    m_allowedTransactions.insert(QnConnectionState::Connecting,     QnConnectionState::Connected);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Ready);
    m_allowedTransactions.insert(QnConnectionState::Ready,          QnConnectionState::Disconnected);

    /* Auto-reconnect. */
    m_allowedTransactions.insert(QnConnectionState::Ready,          QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting,   QnConnectionState::Connected);

    /* Cancelled connect. */
    m_allowedTransactions.insert(QnConnectionState::Connecting,     QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting,   QnConnectionState::Disconnected);
}

QnConnectionState QnClientConnectionStatus::state() const {
    return m_state;
}

void QnClientConnectionStatus::setState(QnConnectionState state) {
#ifdef STRICT_STATE_CONTROL
    qDebug() << "QnClientConnectionStatus::setState" << stateToString(state);
#endif

    if (m_state == state) {
        warn(lit("Duplicated setState %1").arg(stateToString(state)));
        return;
    }

    if (!m_allowedTransactions.values(m_state).contains(state))
        warn(lit("Invalid state transaction %1 -> %2").arg(stateToString(m_state)).arg(stateToString(state)));

    m_state = state;
}

void QnClientConnectionStatus::warn(const QString &message) const {
#ifdef STRICT_STATE_CONTROL
    Q_ASSERT_X(false, Q_FUNC_INFO, message.toUtf8());
#else
    qWarning() << message;
#endif
}

