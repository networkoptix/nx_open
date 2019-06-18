
#include "QtHttpRequest.h"
#include "QtHttpHeader.h"
#include "QtHttpServer.h"

QtHttpRequest::QtHttpRequest (QtHttpClientWrapper * client, QtHttpServer * parent)
    : QObject         (parent)
    , m_url           (QUrl ())
    , m_command       (QString ())
    , m_data          (QByteArray ())
    , m_serverHandle  (parent)
    , m_clientHandle  (client)
{
    // set some additional headers
    addHeader (QtHttpHeader::ContentLength, QByteArrayLiteral ("0"));
    addHeader (QtHttpHeader::Connection,    QByteArrayLiteral ("Keep-Alive"));
}

QtHttpRequest::~QtHttpRequest (void) { }

QUrl QtHttpRequest::getUrl (void) const {
    return m_url;
}

QString QtHttpRequest::getCommand (void) const {
    return m_command;
}

int QtHttpRequest::getRawDataSize (void) const {
    return m_data.size ();
}

QByteArray QtHttpRequest::getRawData (void) const {
    return m_data;
}

QList<QByteArray> QtHttpRequest::getHeadersList (void) const {
    return m_headersHash.keys ();
}

QtHttpClientWrapper * QtHttpRequest::getClient (void) const {
    return m_clientHandle;
}

QByteArray QtHttpRequest::getHeader (const QByteArray & header) const {
    return m_headersHash.value (header, QByteArray ());
}

void QtHttpRequest::setUrl (const QUrl & url) {
    m_url = url;
}

void QtHttpRequest::setCommand (const QString & command) {
    m_command = command;
}

void QtHttpRequest::addHeader (const QByteArray & header, const QByteArray & value) {
    QByteArray key = header.trimmed ();
    if (!key.isEmpty ()) {
        m_headersHash.insert (key, value);
    }
}

void QtHttpRequest::appendRawData (const QByteArray & data) {
    m_data.append (data);
}
