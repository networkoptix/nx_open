#include "log_rest_handler.h"

#include <QtCore/QFile>

#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>

#include <media_server/serverutil.h>

static const int kReadBlockSize = 1024 * 512;

int QnLogRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    qint64 linesToRead = 100;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "lines")
        {
            linesToRead = params[i].second.toLongLong();
            break;
        }
    }

    std::optional<QString> logFilePath;
    {
        const auto name = params.value(lit("name"));
        if (!name.isEmpty())
        {
            if (auto logger = QnLogs::getLogger(name))
                logFilePath = logger->filePath();
        }
        else
        {
            const auto id = params.value(lit("id")).toInt();
            if (auto logger = QnLogs::getLogger(id))
                logFilePath = logger->filePath();
        }
    }

    if (!logFilePath)
    {
        result = "Bad log file id or name";
        return nx::network::http::StatusCode::badRequest;
    }

    if (linesToRead == 0ll)
        linesToRead = 1000000ll;

    QFile f(*logFilePath);
    if (!f.open(QFile::ReadOnly))
    {
        result = NX_FMT("No log entries in %1 (%2)", *logFilePath, f.errorString()).toUtf8();
        return nx::network::http::StatusCode::notFound;
    }
    qint64 fileSize = f.size();

    qint64 currentOffset = qMax(0ll, fileSize - kReadBlockSize);
    qint64 readedLines = 0;
    qint64 totalSize = 0;
    QList<QPair<char*, qint64> > buffers;
    int ignoreHeaderSize = 0;
    while (readedLines <= linesToRead && currentOffset >= 0)
    {
        f.seek(currentOffset);
        char* buffer = new char[kReadBlockSize];
        int readed = f.read(buffer, kReadBlockSize);
        if (readed < 1)
        {
            delete[] buffer;
            break;
        }
        readedLines += QByteArray::fromRawData(buffer, readed).count('\n');
        buffers << QPair<char*, qint64>(buffer, readed);
        currentOffset -= kReadBlockSize;
        totalSize += readed;
    }
    if (!buffers.isEmpty())
    {
        QByteArray tmp = QByteArray::fromRawData(buffers.last().first, buffers.last().second);
        for (; readedLines > linesToRead && ignoreHeaderSize < tmp.size(); readedLines--)
            ignoreHeaderSize = tmp.indexOf('\n', ignoreHeaderSize) + 1;
    }
    ignoreHeaderSize = qMax(0, ignoreHeaderSize);

    QByteArray solidArray;
    solidArray.reserve(totalSize);
    for (int i = buffers.size() - 1; i >= 0; --i)
    {
        if (i == buffers.size() - 1)
            solidArray.append(buffers[i].first + ignoreHeaderSize, buffers[i].second - ignoreHeaderSize);
        else
            solidArray.append(buffers[i].first, buffers[i].second);
        delete buffers[i].first;
    }
    contentType = "text/plain; charset=UTF-8";
    result = solidArray;
    return nx::network::http::StatusCode::ok;
}

int QnLogRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
