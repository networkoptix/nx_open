#include "video_server.h"

QnVideoServer::QnVideoServer():
    QnResource()
    //,m_rtspListener(0)
{

}

QnVideoServer::~QnVideoServer()
{
    //delete m_rtspListener;
}

QString QnVideoServer::getUniqueId() const
{
    QnVideoServer* nonConstThis = const_cast<QnVideoServer*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QString("Server ") + getId().toString();
}

QnAbstractStreamDataProvider* QnVideoServer::createDataProviderInternal(ConnectionRole /*role*/)
{
    return 0;
}

#if 0
void QnVideoServer::startRTSPListener(const QHostAddress& address, int port)
{
    if (m_rtspListener)
        return;
    m_rtspListener = new QnRtspListener(address, port);
}

void QnVideoServer::stopRTSPListener()
{
    delete m_rtspListener;
    m_rtspListener = 0;
}
#endif
