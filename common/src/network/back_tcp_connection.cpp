#include "back_tcp_connection.h"
#include "network/universal_request_processor_p.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "universal_tcp_listener.h"

static const int SOCKET_TIMEOUT = 1000 * 5;
QByteArray BACK_CONNECT_MAGIC = QByteArray::fromHex("C9812BE681304a19A57C6035B11F0A56");


class QnBackTcpConnectionPrivate: public QnUniversalRequestProcessorPrivate
{
public:
    QUrl serverUrl;
};

QnBackTcpConnection::QnBackTcpConnection(const QUrl& serverUrl, QnTcpListener* owner):
    QnUniversalRequestProcessor(new QnBackTcpConnectionPrivate, new TCPSocket(), owner)
{
    Q_D(QnBackTcpConnection);
    d->serverUrl = serverUrl;
    setObjectName( lit("QnBackTcpConnection") );
}

QnBackTcpConnection::~QnBackTcpConnection()
{
    stop();
}

void QnBackTcpConnection::run()
{
    Q_D(QnBackTcpConnection);

    (dynamic_cast<QnUniversalTcpListener*>(d->owner))->onBackConnectSpent(this);

    if (!d->socket->connect(d->serverUrl.host(), d->serverUrl.port(), SOCKET_TIMEOUT))
        return;

    d->socket->setWriteTimeOut(SOCKET_TIMEOUT);
    d->socket->setReadTimeOut(SOCKET_TIMEOUT);

    int totalSend = 0;
    while (!m_needStop && d->socket->isConnected() && totalSend < BACK_CONNECT_MAGIC.length())
    {
        int sended = d->socket->send(BACK_CONNECT_MAGIC.mid(totalSend));
        if (sended > 0)
            totalSend += sended;
    }

    if (totalSend == BACK_CONNECT_MAGIC.length())
        QnUniversalRequestProcessor::run();
}
