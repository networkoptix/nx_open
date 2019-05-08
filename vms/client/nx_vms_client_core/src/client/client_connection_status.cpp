#include "client_connection_status.h"

#include <nx/utils/log/log.h>

static QString toString(QnConnectionState state)
{
    return QnConnectionStateUtils::toString(state);
}

QString QnConnectionStateUtils::toString(QnConnectionState state)
{
    static const QMap<QnConnectionState, QString> stateToString{
        {QnConnectionState::Disconnected, "Disconnected"},
        {QnConnectionState::Connecting, "Connecting"},
        {QnConnectionState::Connected, "Connected"},
        {QnConnectionState::Reconnecting, "Reconnecting"},
        {QnConnectionState::Ready, "Ready"}
    };

    return stateToString[state];
}

QnClientConnectionStatus::QnClientConnectionStatus(QnConnectionState state, QObject* parent):
    QObject(parent),
    m_state(state)
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
    if (!m_allowedTransactions.values(m_state).contains(state))
        NX_WARNING(this, "Invalid state transaction %1 -> %2", m_state, state);

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
