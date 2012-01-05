#include "synchttp.h"

#include <QtCore/QBuffer>
#include <QtCore/QEventLoop>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

SyncHTTP::SyncHTTP(const QUrl &url, QObject *parent)
    : QObject(parent),
      m_url(url)
{
    if (!m_url.userInfo().isEmpty())
        m_credentials = "Basic " + m_url.userInfo().toLatin1().toBase64();
    m_url.setUserInfo(QString());
    m_url.setPath(QString());
}

SyncHTTP::~SyncHTTP ()
{
}

QNetworkReply *SyncHTTP::asyncGet(const QUrl &url, QObject *target, const char *slot)
{
    return asyncRequest(GetOperation, url, 0, target, slot);
}

QNetworkReply *SyncHTTP::asyncPost(const QUrl &url, QIODevice *data, QObject *target, const char *slot)
{
    return asyncRequest(PostOperation, url, data, target, slot);
}

QNetworkReply *SyncHTTP::asyncPost(const QUrl &url, const QByteArray &data, QObject *target, const char *slot)
{
    QBuffer *buffer = new QBuffer;
    buffer->setData(data);
    buffer->open(QIODevice::ReadOnly);

    QNetworkReply *reply = asyncPost(url, buffer, target, slot);
    buffer->setParent(reply);
    return reply;
}

int SyncHTTP::syncGet(const QUrl &url, QIODevice *to)
{
    return syncRequest(GetOperation, url, NULL, to);
}

int SyncHTTP::syncPost(const QUrl &url, QIODevice *data, QIODevice *to)
{
    return syncRequest(PostOperation, url, data, to);
}

int SyncHTTP::syncPost(const QUrl &url, const QByteArray &data, QIODevice *to)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    return syncPost(url, &buffer, to);
}

Q_DECLARE_METATYPE(QNetworkReply::NetworkError)
Q_DECLARE_TYPEINFO(QNetworkReply::NetworkError, Q_PRIMITIVE_TYPE);

QNetworkReply *SyncHTTP::asyncRequest(Operation op, const QUrl &url, QIODevice *data, QObject *target, const char *slot)
{
    qRegisterMetaType<QNetworkReply::NetworkError>();

    QNetworkRequest request(url.isValid() && !url.isRelative() ? url : m_url.resolved(url));
    request.setRawHeader("Connection", "close");
    if (!m_credentials.isEmpty())
        request.setRawHeader("Authorization", m_credentials);

    QNetworkAccessManager *accessManager = new QNetworkAccessManager; // ###
    connect(accessManager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(onAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
    connect(accessManager, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
            this, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
    if (target && slot)
        connect(accessManager, SIGNAL(finished(QNetworkReply*)), target, slot); //, Qt::UniqueConnection);

    QNetworkReply *reply = 0;
    switch (op) {
    case GetOperation:
        reply = accessManager->get(request);
        break;
    case PostOperation:
        reply = accessManager->post(request, data);
        break;
    default:
        qWarning("SyncHTTP::asyncRequest(): Unsupported operation");
        break;
    }

    if (reply) {
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(destroyed()), accessManager, SLOT(deleteLater()));
    } else {
        accessManager->deleteLater();
    }

    return reply;
}

int SyncHTTP::syncRequest(Operation op, const QUrl &url, QIODevice *data, QIODevice *outgoingData)
{
    QEventLoop loop;

    QNetworkReply *reply = asyncRequest(op, url, data, &loop, SLOT(quit()));

    loop.exec();

    int error = reply->error();
    if (outgoingData)
        outgoingData->write(reply->readAll());
    return error;
}

void SyncHTTP::onError(QNetworkReply::NetworkError err)
{
    Q_EMIT error(int(err));
}

void SyncHTTP::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    Q_EMIT authenticationRequired(authenticator);
}
