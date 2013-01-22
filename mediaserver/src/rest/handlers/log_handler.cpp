#include "log_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"

static const int READ_BLOCK_SIZE = 1024*512;

extern "C" {
    quint32 crc32 (quint32 crc, char *buf, quint32 len);
}

QnRestLogHandler::QnRestLogHandler()
{

}

int QnRestLogHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding)
{
    bool enableGZip = true; // todo: check accept-encoding here

    qint64 linesToRead = 100;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "lines")
        {
            linesToRead = params[i].second.toLongLong();
            break;
        }
    }

    QString fileName = QnLog::instance()->logFileName();
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly))
    {
        result.append(QString("<root>Can't open log file</root>\n"));
        return CODE_INTERNAL_ERROR;
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
    if (!enableGZip) {
        result = solidArray;
        return CODE_OK;
    }

    static int QT_HEADER_SIZE = 4;
    static int ZLIB_HEADER_SIZE = 2;
    static int ZLIB_SUFFIX_SIZE = 4;
    static int GZIP_HEADER_SIZE = 10;
    const char GZIP_HEADER[] = {
        0x1f, 0x8b      // gzip magic number
        , 8             // compress method "defalte"
        , 1             // text data
        , 0, 0, 0, 0    // timestamp is not set
        , 2             // maximum compression flag
        , 255           // unknown OS
    };



    QByteArray compressedData = qCompress(solidArray);
    QByteArray cleanData = QByteArray::fromRawData(compressedData.data() + QT_HEADER_SIZE + ZLIB_HEADER_SIZE, 
                                                   compressedData.size() - (QT_HEADER_SIZE + ZLIB_HEADER_SIZE + ZLIB_SUFFIX_SIZE));
    result.reserve(cleanData.size() + GZIP_HEADER_SIZE);
    result.append(GZIP_HEADER, GZIP_HEADER_SIZE);
    result.append(cleanData);
    int tmp = crc32(0, cleanData.data(), cleanData.size());
    result.append((const char*) &tmp, sizeof(int));
    tmp = cleanData.size();
    result.append((const char*) &tmp, sizeof(int));
    contentEncoding = "gzip";

    return CODE_OK;
}

int QnRestLogHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType, contentEncoding);
}

QString QnRestLogHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns tail of the server log file";
    rez += "<BR>Param <b>lines</b> - Optional. Display last N log lines.";
    return rez;
}
