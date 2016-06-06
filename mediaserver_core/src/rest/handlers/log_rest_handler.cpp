#include "log_rest_handler.h"

#include <QtCore/QFile>

#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <nx/utils/log/log.h>

#include <media_server/serverutil.h>

static const int READ_BLOCK_SIZE = 1024*512;

namespace {
const QString idParam = lit("id");
}

int QnLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    qint64 linesToRead = 100;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "lines")
        {
            linesToRead = params[i].second.toLongLong();
            break;
        }
    }

    int logId = 0;
    const auto idIter = params.find(idParam);
    if (idIter != params.end())
        logId = idIter->second.toInt();

    if (!QnLog::logs()->exists(logId))
    {
        result.append(QString("<root>Bad log file id</root>\n"));
        return nx_http::StatusCode::badRequest;
    }

    if (linesToRead == 0ll)
        linesToRead = 1000000ll;

    QString fileName = QnLog::logFileName(logId);
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly))
    {
        result.append(QString("<root>Can't open log file</root>\n"));
        return nx_http::StatusCode::internalServerError;
    }
    qint64 fileSize = f.size();

    qint64 currentOffset = qMax(0ll, fileSize - READ_BLOCK_SIZE);
    qint64 readedLines = 0;
    qint64 totalSize = 0;
    QList<QPair<char*, qint64> > buffers;
    int ignoreHeaderSize = 0;
    while (readedLines <= linesToRead && currentOffset >= 0)
    {
        f.seek(currentOffset);
        char* buffer = new char[READ_BLOCK_SIZE];
        int readed = f.read(buffer, READ_BLOCK_SIZE);
        if (readed < 1) {
            delete [] buffer;
            break;
        }
        readedLines += QByteArray::fromRawData(buffer, readed).count('\n');
        buffers << QPair<char*, qint64>(buffer, readed);
        currentOffset -= READ_BLOCK_SIZE;
        totalSize += readed;
    }
    if (!buffers.isEmpty()) {
        QByteArray tmp = QByteArray::fromRawData(buffers.last().first, buffers.last().second);
        for (;readedLines > linesToRead && ignoreHeaderSize < tmp.size(); readedLines--)
            ignoreHeaderSize = tmp.indexOf('\n', ignoreHeaderSize)+1;
    }
    ignoreHeaderSize = qMax(0, ignoreHeaderSize);

    QByteArray solidArray;
    solidArray.reserve(totalSize);
    for (int i = buffers.size()-1; i >=0; --i)
    {
        if (i == buffers.size()-1)
            solidArray.append(buffers[i].first+ignoreHeaderSize, buffers[i].second - ignoreHeaderSize);
        else
            solidArray.append(buffers[i].first, buffers[i].second);
        delete buffers[i].first;
    }
    contentType = "text/plain; charset=UTF-8";
    result = solidArray;
    return nx_http::StatusCode::ok;
}

int QnLogRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                  QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
