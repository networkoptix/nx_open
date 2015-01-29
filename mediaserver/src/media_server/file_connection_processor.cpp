#include <QCache>
#include <QFileInfo>
#include "file_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/util.h"
#include "media_server/serverutil.h"
#include "utils/gzip/gzip_compressor.h"


static const qint64 CACHE_SIZE = 1024 * 256;

struct CacheEntry
{
    CacheEntry(const QByteArray& data, const QDateTime& lastModified): data(data), lastModified(lastModified) {}

    QByteArray data;
    QDateTime lastModified;
};


static QCache<QString, CacheEntry> cachedFiles(CACHE_SIZE);
static QMutex cacheMutex;

struct HttpContentTypes
{
    const char* fileExt;
    const char* contentType;
};

static HttpContentTypes contentTypes[] =
{
    { "html", "text/html"},
    { "htm",  "text/html"},
    { "css",  "text/css"},
    { "js",   "application/javascript"},
    { "json", "application/json"},
    { "png",  "image/png"},
    { "jpeg", "image/jpeg"},
    { "jpg",  "image/jpeg"},
    { "xml",  "application/xml" },
    { "xsl",  "applicaton/xslt+xml" },
    { "zip",  "application/zip" }
};


class QnFileConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
};

QnFileConnectionProcessor::QnFileConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* /*_owner*/):
    QnTCPConnectionProcessor(new QnFileConnectionProcessorPrivate, socket)
{
}

QnFileConnectionProcessor::~QnFileConnectionProcessor()
{
    stop();
}

QByteArray QnFileConnectionProcessor::readStaticFile(const QString& path)
{
    QString fileName = getDataDirectory() + "/web/" + path;
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        file.setFileName(QString(":") + path); // check file into internal resources
        if (!file.open(QFile::ReadOnly))
            return QByteArray();
    }
    return file.readAll();
}

void QnFileConnectionProcessor::run()
{
    Q_D(QnFileConnectionProcessor);

    initSystemThreadId();

    if (d->clientRequest.isEmpty()) {
        if (!readRequest())
            return;
    }
    parseRequest();


    d->responseBody.clear();

    QUrl url = getDecodedUrl();
    QString path = url.path();

    int rez = CODE_OK;
    QByteArray contentType = "application/xml";
    QByteArray contentEncoding;
    
    QString fileName = getDataDirectory() + "/web/" + path;
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        file.setFileName(QString(":") + path); // check file into internal resources
        if (!file.open(QFile::ReadOnly)) {
            rez = CODE_NOT_FOUND;
        }
    }

    QDateTime lastModified = QFileInfo(file.fileName()).lastModified();
    QString modifiedSinceStr = nx_http::getHeaderValue(d->request.headers, "If-Modified-Since");
    if (!modifiedSinceStr.isEmpty()) {
        QDateTime modifiedSince = QDateTime::fromString(modifiedSinceStr, Qt::RFC2822Date);
        if (lastModified <= modifiedSince)
            rez = CODE_NOT_MODIFIED;
    }

    if (rez == CODE_OK)
    {
        QMutexLocker lock(&cacheMutex);

        CacheEntry* cachedData = cachedFiles.object(path);
        if (cachedData && cachedData->lastModified == lastModified)
        {
            d->responseBody = cachedData->data;
        }
        else 
        {
            d->responseBody = file.readAll();
            if (d->responseBody.size() < cachedFiles.maxCost())
                cachedFiles.insert(path, new CacheEntry(d->responseBody, lastModified), d->responseBody.size());
        }

        int formats = sizeof(contentTypes) / sizeof(HttpContentTypes);
        QString fileFormat = QFileInfo(path).suffix().toLower();
        for (int i = 0; i < formats; ++i)
        {
            if (contentTypes[i].fileExt == fileFormat) {
                contentType = QByteArray(contentTypes[i].contentType);
            }
        }

#ifndef EDGE_SERVER
        if ( nx_http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->responseBody.isEmpty()) 
        {
            if (!contentType.contains("image")) {
                d->responseBody = GZipCompressor::compressData(d->responseBody);
                contentEncoding = "gzip";
            }
        }
#endif
    }
    d->response.headers.insert(nx_http::HttpHeader("Last-Modified", dateTimeToHTTPFormat(lastModified).toUtf8()));
    sendResponse(rez, contentType, contentEncoding, false);
}
