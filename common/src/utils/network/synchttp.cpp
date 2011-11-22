#include "synchttp.h"

namespace {
    struct Poster {
        QNetworkReply *operator()(QNetworkAccessManager *manager, const QNetworkRequest &request, QIODevice *data) const {
            return manager->post(request, data);
        }
    };

    struct Getter {
        QNetworkReply *operator()(QNetworkAccessManager *manager, const QNetworkRequest &request, QIODevice *) const {
            return manager->get(request);
        }
    };
}

SyncHTTP::SyncHTTP (const QHostAddress & hostName, quint16 port, QAuthenticator auth)
    : m_hostName(hostName),
      m_port(port)
{
    m_credentials = auth.user() + ":" + auth.password();
    m_credentials = "Basic " + m_credentials.toAscii().toBase64();
}

SyncHTTP::~SyncHTTP ()
{
}

template<class Sender>
int SyncHTTP::syncRequest(const Sender &sender, const QString &path, QIODevice *data, QIODevice *to) 
{
    /* Prepare request. */
    QNetworkRequest request(QUrl("http://" + m_hostName.toString() + ":" + QString::number(m_port) + "/" + path));
    request.setRawHeader(QByteArray("Authorization"), m_credentials.toAscii());

    /* Prepare network & sync machinery. */
    QNetworkAccessManager accessManager;
    QEventLoop loop;
    connect(&accessManager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));

    /* Send request. */
    QScopedPointer<QNetworkReply> reply(sender(&accessManager, request, data));

    /* Wait for the request to finish. */
    loop.exec();

    /* Prepare result. */
    int error = reply->error();
    if(to != NULL)
        to->write(reply->readAll());
    return error;
}

int SyncHTTP::syncGet(const QString &path, QIODevice *to)
{
    return syncRequest(Getter(), path, NULL, to);
}

int SyncHTTP::syncPost(const QString & path, QIODevice * data, QIODevice * to)
{
    return syncRequest(Poster(), path, data, to);
}

int SyncHTTP::syncPost(const QString& path, const QByteArray& data, QIODevice* to)
{
    QBuffer buffer;
    buffer.setData(data);
    return syncPost(path, &buffer, to);
}

template<class Sender>
void SyncHTTP::asyncRequest(const Sender &sender, const QString &path, QIODevice *data, QObject *target, const char *slot) 
{
    /* Prepare request. */
    QNetworkRequest request(QUrl("http://" + m_hostName.toString() + ":" + QString::number(m_port) + "/" + path));
    request.setRawHeader(QByteArray("Authorization"), m_credentials.toAscii());

    /* Prepare network machinery. */
    QNetworkAccessManager *accessManager = new QNetworkAccessManager();
    connect(accessManager, SIGNAL(finished(QNetworkReply*)), accessManager, SLOT(deleteLater()));
    connect(accessManager, SIGNAL(finished(QNetworkReply*)), target, slot);

    /* Send request. */
    sender(accessManager, request, data);
}

void SyncHTTP::asyncGet(const QString &path, QObject *target, const char *slot) 
{
    asyncRequest(Getter(), path, NULL, target, slot);
}

void SyncHTTP::asyncPost(const QString &path, QIODevice *data, QObject *target, const char *slot) 
{
    asyncRequest(Poster(), path, data, target, slot);
}

void SyncHTTP::asyncPost(const QString &path, const QByteArray &data, QObject *target, const char *slot) 
{
    QBuffer buffer;
    buffer.setData(data);
    return asyncPost(path, &buffer, target, slot);
}
