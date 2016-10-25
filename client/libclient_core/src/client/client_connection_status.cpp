#include "client_connection_status.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

//#define STRICT_STATE_CONTROL
#define DEBUG_CLIENT_CONNECTION_STATUS
#ifdef DEBUG_CLIENT_CONNECTION_STATUS
#define TRACE(...) qDebug() << "QnClientConnectionStatus: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

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

QnClientConnectionStatus::QnClientConnectionStatus(QnConnectionState state, QObject* parent)
    :
    QObject(parent),
    m_state(state),
    m_allowedTransactions()
{
    /* Default way. */
    m_allowedTransactions.insert(QnConnectionState::Disconnected, QnConnectionState::Connecting);
    m_allowedTransactions.insert(QnConnectionState::Connecting,   QnConnectionState::Connected);
    m_allowedTransactions.insert(QnConnectionState::Connected,    QnConnectionState::Ready);

    /* Auto-reconnect. */
    m_allowedTransactions.insert(QnConnectionState::Ready,        QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Connected,    QnConnectionState::Reconnecting);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting, QnConnectionState::Connected);

    /* Cancelled connect. */
    m_allowedTransactions.insert(QnConnectionState::Connecting,   QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Connected,    QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Reconnecting, QnConnectionState::Disconnected);
    m_allowedTransactions.insert(QnConnectionState::Ready,        QnConnectionState::Disconnected);

    /* Double-checked disconnect is allowed. */
    m_allowedTransactions.insert(QnConnectionState::Disconnected, QnConnectionState::Disconnected);
}

QnConnectionState QnClientConnectionStatus::state() const
{
    return m_state;
}

void QnClientConnectionStatus::setState(QnConnectionState state)
{
    //TRACE("setState" << stateToString[state]);
    if (!m_allowedTransactions.values(m_state).contains(state))
        warn(lit("Invalid state transaction %1 -> %2").arg(stateToString[m_state]).arg(stateToString[state]));

    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

bool QnClientConnectionStatus::operator!=(QnConnectionState state) const
{
    return m_state != state;
}

QnClientConnectionStatus& QnClientConnectionStatus::operator=(QnConnectionState state)
{
    setState(state);
    return *this;
}

bool QnClientConnectionStatus::operator==(QnConnectionState state) const
{
    return m_state == state;
}

void QnClientConnectionStatus::warn(const QString &message) const
{
    NX_LOG(message, cl_logWARNING);
    TRACE(message);
#ifdef STRICT_STATE_CONTROL
    NX_EXPECT(false, Q_FUNC_INFO, message.toUtf8());
#endif
}

