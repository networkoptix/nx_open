#include "file_connection_processor.h"

#include <QtCore/QCache>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/gzip/gzip_compressor.h>

#include <media_server/serverutil.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/util.h>
#include <network/tcp_listener.h>
#include <common/common_module.h>

QString QnFileConnectionProcessor::externalPackagePath()
{
    //static const QString path = QDir(qApp->applicationDirPath()).filePath(lit("external.dat"));
    static const QString path = QDir(lit("/opt/hanwha/mediaserver/bin")).filePath(lit("external.dat"));
    return path;
}

namespace {
    static const qint64 CACHE_SIZE = 1024 * 1024;

    struct CacheEntry
    {
        CacheEntry(const QByteArray& data, const QDateTime& lastModified) : data(data), lastModified(lastModified) {}

        QByteArray data;
        QDateTime lastModified;
    };


    static QCache<QString, CacheEntry> cachedFiles(CACHE_SIZE);
    static QCache<QString, int> externalPackageNameMiss(CACHE_SIZE);
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
        { "swf",  "application/x-shockwave-flash" },
        { "txt",  "text/plain" }
    };

    const QByteArray kDefaultContentType = "text/html; charset=utf-8";

    QIODevicePtr getStaticFile(const QString& relativePath)
    {
        if (!externalPackageNameMiss.contains(relativePath))
        {
            QIODevicePtr result(new QuaZipFile(QnFileConnectionProcessor::externalPackagePath(), relativePath));
            if (result->open(QuaZipFile::ReadOnly))
                return result;

            externalPackageNameMiss.insert(relativePath, nullptr);
        }

        /* Check internal resources. */
        QString fileName = ":" + relativePath;
        QIODevicePtr result(new QFile(fileName));
        if (result->open(QFile::ReadOnly))
            return result;

        return QIODevicePtr();
    }

    QDateTime staticFileLastModified(const QIODevicePtr& device)
    {
        QDateTime result;
        if (QFile* file = qobject_cast<QFile*>(device.get()))
            result = QFileInfo(*file).lastModified();
        else
            result = QFileInfo(QnFileConnectionProcessor::externalPackagePath()).lastModified();

        // zero msec in result
        return result.addMSecs(-result.time().msec());
    }
}


class QnFileConnectionProcessorPrivate : public QnTCPConnectionProcessorPrivate
{
public:
};

QnFileConnectionProcessor::QnFileConnectionProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner)
:
    QnTCPConnectionProcessor(new QnTCPConnectionProcessorPrivate, socket, owner)
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

bool QnFileConnectionProcessor::loadFile(
    const QString& path,
    QDateTime* outLastModified,
    QByteArray* outData)
{
    QnMutexLocker lock(&cacheMutex);

    CacheEntry* cachedData = cachedFiles.object(path);
    if (cachedData)
    {
        *outData = cachedData->data;
        *outLastModified = cachedData->lastModified;
        return true;
    }

    QIODevicePtr file = getStaticFile(path);
    if (!file)
        return false;

    *outData = file->readAll();
    *outLastModified = commonModule()->startupTime(); //staticFileLastModified(file);
    if (outData->size() < cachedFiles.maxCost())
        cachedFiles.insert(path, new CacheEntry(*outData, *outLastModified), outData->size());
    return true;
}

QByteArray QnFileConnectionProcessor::compressMessageBody(const QByteArray& contentType)
{
    Q_D(QnFileConnectionProcessor);
#ifndef EDGE_SERVER
    if (nx::network::http::getHeaderValue(d->request.headers, "Accept-Encoding").toLower().contains("gzip") && !d->response.messageBody.isEmpty())
    {
        if (!contentType.contains("image")) {
            d->response.messageBody = nx::utils::bstream::gzip::Compressor::compressData(d->response.messageBody);
            return "gzip";
        }
    }
#endif
    return QByteArray();
}

void QnFileConnectionProcessor::run()
{
    Q_D(QnFileConnectionProcessor);
    initSystemThreadId();

    if (d->clientRequest.isEmpty() && !readRequest())
        return;
    parseRequest();
    d->response.messageBody.clear();

    nx::utils::Url url = getDecodedUrl();
    QString path = QString('/') + QnTcpListener::normalizedPath(url.path());
    const QString fileFormat = QFileInfo(path).suffix().toLower();
    QByteArray contentType = contentTypes.value(fileFormat, kDefaultContentType);

    if (path.contains(".."))
    {
        sendResponse(nx::network::http::StatusCode::forbidden, contentType, QByteArray());
        return;
    }

    QDateTime lastModified;
    if (!loadFile(path, &lastModified, &d->response.messageBody))
    {
        sendResponse(nx::network::http::StatusCode::notFound, contentType, QByteArray());
        return;
    }

    nx::network::http::HttpHeader modifiedHeader("Last-Modified", nx::network::http::formatDateTime(lastModified));
    d->response.headers.insert(modifiedHeader);
    QString modifiedSinceStr = nx::network::http::getHeaderValue(d->request.headers, "If-Modified-Since");
    if (!modifiedSinceStr.isEmpty())
    {
        QDateTime modifiedSince = QDateTime::fromString(modifiedSinceStr, Qt::RFC2822Date);
        if (lastModified <= modifiedSince)
        {
            sendResponse(nx::network::http::StatusCode::notModified, contentType, QByteArray());
            return;
        }
    }

    sendResponse(nx::network::http::StatusCode::ok, contentType, compressMessageBody(contentType));
}
