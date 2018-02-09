#include "qtsingleguiapplication.h"
#include "qtlocalpeer.h"

QtSingleGuiApplication::QtSingleGuiApplication(int& argc, char** argv):
    QGuiApplication(argc, argv),
    m_peer(new QtLocalPeer(this))
{
    connect(m_peer, &QtLocalPeer::messageReceived, this, &QtSingleGuiApplication::messageReceived);
}

QtSingleGuiApplication::QtSingleGuiApplication(const QString& appId, int& argc, char** argv):
    QGuiApplication(argc, argv),
    m_peer(new QtLocalPeer(this, appId))
{
    connect(m_peer, &QtLocalPeer::messageReceived, this, &QtSingleGuiApplication::messageReceived);
}

bool QtSingleGuiApplication::isRunning()
{
    return m_peer->isClient();
}

bool QtSingleGuiApplication::sendMessage(const QString& message, int timeout)
{
    return m_peer->sendMessage(message, timeout);
}

QString QtSingleGuiApplication::id() const
{
    return m_peer->applicationId();
}
