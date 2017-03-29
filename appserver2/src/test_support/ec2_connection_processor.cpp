#include "ec2_connection_processor.h"

#include <QtCore/QElapsedTimer>

#include <network/http_connection_listener.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>

Ec2ConnectionProcessor::Ec2ConnectionProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket, owner),
    m_processor(nullptr)
{
    setObjectName(QLatin1String("Ec2ConnectionProcessor"));
}

Ec2ConnectionProcessor::~Ec2ConnectionProcessor()
{
    stop();
}

void Ec2ConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);

    initSystemThreadId();

    if (!readRequest())
        return;

    QElapsedTimer t;
    t.restart();
    bool ready = true;
    bool isKeepAlive = false;

    while (1)
    {
        if (ready)
        {
            t.restart();
            parseRequest();

            bool noAuth = true;

            isKeepAlive = isConnectionCanBePersistent();


            // getting a new handler inside is necessary due to possibility of
            // changing request during authentication
            if (!processRequest(noAuth))
            {
                QByteArray contentType;
                sendResponse(nx_http::StatusCode::badRequest, contentType);
            }
        }

        if (!d->socket)
            break; // processor has token socket ownership

        if (!isKeepAlive || t.elapsed() >= KEEP_ALIVE_TIMEOUT || !d->socket->isConnected())
            break;

        ready = readRequest();
    }
    if (d->socket)
        d->socket->close();
}

bool Ec2ConnectionProcessor::processRequest(bool noAuth)
{
    Q_D(QnTCPConnectionProcessor);

    QnMutexLocker lock(&m_mutex);
    QnHttpConnectionListener* owner = (QnHttpConnectionListener*) d->owner;
    if (auto handler = owner->findHandler(d->protocol, d->request))
        m_processor = handler(d->socket, owner);
    else
        return false;

    d->accessRights = Qn::kSystemAccess;

    auto restProcessor = dynamic_cast<QnRestConnectionProcessor*>(m_processor);
    if (restProcessor)
        restProcessor->setAuthNotRequired(noAuth);
    if (!needToStop())
    {
        copyClientRequestTo(*m_processor);
        if (m_processor->isTakeSockOwnership())
            d->socket.clear(); // some of handlers have addition thread and depend of socket destructor. We should clear socket immediately to prevent race condition
        m_processor->execute(m_mutex);
        if (!m_processor->isTakeSockOwnership())
            m_processor->releaseSocket();
        else
            d->socket.clear(); // some of handlers set ownership dynamically during a execute call. So, check it again.
    }

    delete m_processor;
    m_processor = nullptr;

    return true;
}

void Ec2ConnectionProcessor::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    QnTCPConnectionProcessor::pleaseStop();
}
