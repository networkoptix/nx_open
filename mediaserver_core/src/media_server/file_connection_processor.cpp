#include "file_connection_processor.h"

#include <QtCore/QCache>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <media_server/serverutil.h>

#include <network/tcp_connection_priv.h>
#include <nx/network/http/httptypes.h>

#include <utils/gzip/gzip_compressor.h>
#include <utils/common/util.h>

namespace {
    static const qint64 CACHE_SIZE = 1024 * 256;

    struct CacheEntry
    {
        CacheEntry(const QByteArray& data, const QDateTime& lastModified) : data(data), lastModified(lastModified) {}

        QByteArray data;
        QDateTime lastModified;
    };


    static QCache<QString, CacheEntry> cachedFiles(CACHE_SIZE);
    static QnMutex cacheMutex;

    static QHash<QString, QByteArray> contentTypes = {
        { "html", "text/html; charset=utf-8" },
        { "htm",  "text/html; charset=utf-8" },
        { "css",  "text/css" },
        { "js",   "application/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },
        { "jpeg", "image/jpeg" },
        { "jpg",  "image/jpeg" },
        { "xml",  "application/xml" },
        { "xsl",  "applicaton/xslt+xml" },
        { "zip",  "application/zip" },
        { "swf",  "application/x-shockwave-flash" }
    };

    const QByteArray kDefaultContentType = "application/xml";

    const QString kExternalResourcesPackageName = "external.dat";

    typedef std::unique_ptr<QIODevice> QIODevicePtr;

    QIODevicePtr getStaticFile(const QString& relativePath)
    {
        {   /* Check external package. */
            static const QString packageName = QDir(qApp->applicationDirPath()).filePath(kExternalResourcesPackageName);
            QIODevicePtr result(new QuaZipFile(packageName, relativePath));
            if (result->open(QuaZipFile::ReadOnly))
                return result;
        }

        {   /* Check internal resources. */
            QString fileName = ":" + relativePath;
            QIODevicePtr result(new QFile(fileName));
            if (result->open(QFile::ReadOnly))
                return result;
        }

        return QIODevicePtr();
    }

    QDateTime staticFileLastModified(QIODevice* device)
    {
        if (!device)
            return QDateTime();

        if (QFile* file = qobject_cast<QFile*>(device))
            return QFileInfo(*file).lastModified();

        static const QString packageName = QDir(qApp->applicationDirPath()).filePath(kExternalResourcesPackageName);
        return QFileInfo(packageName).lastModified();
    }
}


class QnFileConnectionProcessorPrivate : public QnTCPConnectionProcessorPrivate
{
public:
};

QnFileConnectionProcessor::QnFileConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* /*_owner*/):
    QnTCPConnectionProcessor(new QnTCPConnectionProcessorPrivate, socket)
{
}

QnFileConnectionProcessor::~QnFileConnectionProcessor()
{
    stop();
}

QByteArray QnFileConnectionProcessor::readStaticFile(const QString& path)
{
    QIODevicePtr file = getStaticFile(path);
    if (file)
        return file->readAll();
    return QByteArray();
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


    d->response.messageBody.clear();

    QUrl url = getDecodedUrl();
    QString path = url.path();
    while (path.endsWith(QLatin1String("/")))
        path.chop(1);

    int rez = nx_http::StatusCode::ok;
    QByteArray contentType = kDefaultContentType;
    QByteArray contentEncoding;

    QIODevicePtr file = getStaticFile(path);
    if (!file)
        rez = nx_http::StatusCode::notFound;

    QDateTime lastModified = staticFileLastModified(file.get());
    QString modifiedSinceStr = nx_http::getHeaderValue(d->request.headers, "If-Modified-Since");
    if (!modifiedSinceStr.isEmpty())
    {
        QDateTime modifiedSince = QDateTime::fromString(modifiedSinceStr, Qt::RFC2822Date);
        if (lastModified <= modifiedSince)
            rez = nx_http::StatusCode::notModified;
        //TODO: #rvasilenko invalid content type will be returned here
    }

    if (rez == nx_http::StatusCode::ok)
    {
        QnMutexLocker lock( &cacheMutex );

        CacheEntry* cachedData = cachedFiles.object(path);
        if (cachedData && cachedData->lastModified == lastModified)
        {
            d->response.messageBody = cachedData->data;
        }
        else
        {
            d->response.messageBody = file->readAll();
#ifndef _DEBUG
            if (d->response.messageBody.size() < cachedFiles.maxCost())
                cachedFiles.insert(path, new CacheEntry(d->response.messageBody, lastModified), d->response.messageBody.size());
#endif
        }

        const QString fileFormat = QFileInfo(path).suffix().toLower();
        contentType = contentTypes.value(fileFormat, kDefaultContentType);

#ifndef EDGE_SERVER
        if ( nx_http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->response.messageBody.isEmpty())
        {
            if (!contentType.contains("image")) {
                d->response.messageBody = GZipCompressor::compressData(d->response.messageBody);
                contentEncoding = "gzip";
            }
        }
#endif
    }
    d->response.headers.insert(nx_http::HttpHeader("Last-Modified", dateTimeToHTTPFormat(lastModified)));
    sendResponse(rez, contentType, contentEncoding);
}
