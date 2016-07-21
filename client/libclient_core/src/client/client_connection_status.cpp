#include "client_connection_status.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

//#define STRICT_STATE_CONTROL

namespace {

const QMap<QnConnectionState, QString> stateToString
{
    {QnConnectionState::Disconnected,   lit("Disconnected")},
    {QnConnectionState::Connecting,     lit("Connecting")},
    {QnConnectionState::Connected,      lit("Connected")},
    {QnConnectionState::Reconnecting,   lit("Reconnecting")},
    {QnConnectionState::Ready,          lit("Ready")}
};

} //anonymous namespace

QString QnConnectionStateUtils::toString(QnConnectionState state)
{
    return stateToString[state];
}

QnClientConnectionStatus::QnClientConnectionStatus(QnConnectionState state)
    :
    m_state(state),
    m_allowedTransactions()
{
    /* Default way. */
    m_allowedTransactions.insert(QnConnectionState::Disconnected,   QnConnectionState::Connecting);
    m_allowedTransactions.insert(QnConnectionState::Connecting,     QnConnectionState::Connected);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Ready);
    m_allowedTransactions.insert(QnConnectionState::Ready,          QnConnectionState::Disconnected);

    /* Auto-reconnect. */
    m_allowedTransactions.insert(QnConnectionState::Ready,          QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting,   QnConnectionState::Connected);      /*< Reconnected to current server. */
    m_allowedTransactions.insert(QnConnectionState::Reconnecting,   QnConnectionState::Connecting);     /*< Trying another server. */

    /* Cancelled connect. */
    m_allowedTransactions.insert(QnConnectionState::Connecting,     QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Connected,      QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting,   QnConnectionState::Disconnected);

    /* Double-checked disconnect is allowed. */
    m_allowedTransactions.insert(QnConnectionState::Disconnected,   QnConnectionState::Disconnected);
}

QnConnectionState QnClientConnectionStatus::state() const
{
    return m_state;
}

void QnClientConnectionStatus::setState(QnConnectionState state)
{
#ifdef STRICT_STATE_CONTROL
    qDebug() << "QnClientConnectionStatus::setState" << stateToString[m_state];
#endif

    if (!m_allowedTransactions.values(m_state).contains(state))
        warn(lit("Invalid state transaction %1 -> %2").arg(stateToString[m_state]).arg(stateToString[state]));

    m_state = state;
}

void QnClientConnectionStatus::warn(const QString &message) const
{
    NX_LOG(message, cl_logWARNING);
#ifdef STRICT_STATE_CONTROL
    NX_EXPECT(false, Q_FUNC_INFO, message.toUtf8());
#else
    qWarning() << message;
#endif
}

