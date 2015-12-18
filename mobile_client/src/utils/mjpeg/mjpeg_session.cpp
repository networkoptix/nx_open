#include "mjpeg_session.h"

#include <QtCore/QUrl>
#include <QtCore/QQueue>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <utils/thread/mutex.h>

#if defined(Q_OS_IOS) || defined(Q_OS_MAC) || defined(Q_OS_ANDROID)
#define USE_LIBJPEG
#endif

#ifdef USE_LIBJPEG
#include <libjpeg/jpeglib.h>
#endif

namespace {
    const qint64 maxHttpBufferSize = 2 * 1024 * 1024;
    const qint64 maxFrameQueueSize = 8;

#ifdef USE_LIBJPEG
    void imageCleanup(void *info) {
        delete [](uchar*)info;
    }

    QImage decompressJpegImage(const char *data, size_t size) {
        jpeg_decompress_struct jinfo;
        jpeg_error_mgr jerr;

        jinfo.err = jpeg_std_error(&jerr);

        jpeg_create_decompress(&jinfo);
        jpeg_mem_src(&jinfo, (uchar*)data, size);
        jpeg_read_header(&jinfo, TRUE);
        jinfo.out_color_space = JCS_EXT_BGRX;

        jpeg_start_decompress(&jinfo);

        int width = jinfo.output_width;
        int height = jinfo.output_height;
        int bytesPerLine = jinfo.output_width * jinfo.output_components;
        uchar *buffer = new uchar[bytesPerLine * height];

        while (jinfo.output_scanline < jinfo.output_height) {
            uchar *line = buffer + bytesPerLine * jinfo.output_scanline;
            jpeg_read_scanlines(&jinfo, &line, 1);
        }

        jpeg_finish_decompress(&jinfo);
        jpeg_destroy_decompress(&jinfo);

        return QImage(buffer, width, height, QImage::Format_ARGB32, &imageCleanup, buffer);
    }
#else
    QImage decompressJpegImage(const char *data, size_t size) {
        return QImage::fromData(reinterpret_cast<const uchar*>(data), size);
    }
#endif
}

class QnMjpegSessionPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegSession)
public:
    struct FrameData {
        int presentationTime;
        qint64 timestamp;
        QImage image;
    };

    QnMjpegSession *q_ptr;

    mutable QnMutex mutex;

    QUrl url;
    QByteArray previousFrameData;
    qint64 previousFrameTimestampUSec;
    qint64 frameTimestampUSec;
    QQueue<FrameData> frameQueue;
    QSemaphore queueSemaphore;

    qint64 finalTimestampMs;

    QNetworkAccessManager *networkAccessManager;
    QNetworkReply *reply;

    QnMjpegSession::State state;

    QByteArray boundary;
    QByteArray httpBuffer;
    qint64 bodyLength;

    bool processing;

    enum ParserState {
        ParseBoundary,
        ParseHeaders,
        ParseBody
    };

    enum ParseResult {
        ParseOk,
        ParseNeedMoreData,
        ParseFailed
    };

    ParserState parserState;

    QnMjpegSessionPrivate(QnMjpegSession *parent);

    void setState(QnMjpegSession::State state);

    void decodeAndEnqueueFrame(const QByteArray &data, qint64 timestampMs, int presentationTime);
    bool dequeueFrame(QImage *image, qint64 *timestamp, int *presentationTime);

    bool connect();
    void disconnect();

    void at_reply_readyRead();
    void at_reply_error();
    void at_reply_finished();

    bool processHeaders();

    ParseResult processBuffer();
    ParseResult parseBoundary();
    ParseResult parseHeaders();
    ParseResult parseBody();
};

QnMjpegSessionPrivate::QnMjpegSessionPrivate(QnMjpegSession *parent)
    : QObject(parent)
    , q_ptr(parent)
    , queueSemaphore(maxFrameQueueSize)
    , finalTimestampMs(-1)
    , networkAccessManager(nullptr)
    , reply(nullptr)
    , state(QnMjpegSession::Stopped)
    , processing(false)
    , parserState(ParseBoundary)
{}

void QnMjpegSessionPrivate::setState(QnMjpegSession::State state) {
    {
        QnMutexLocker lock(&mutex);

        if (this->state == state)
            return;

        this->state = state;
    }

    Q_Q(QnMjpegSession);
    emit q->stateChanged();
}

void QnMjpegSessionPrivate::decodeAndEnqueueFrame(const QByteArray &data, qint64 timestampMs, int presentationTime) {
    FrameData frameData;
    frameData.image = decompressJpegImage(data.data(), data.size());
    frameData.timestamp = timestampMs;
    frameData.presentationTime = presentationTime;

    queueSemaphore.acquire();
    {
        QnMutexLocker lock(&mutex);
        if (state != QnMjpegSession::Playing) {
            queueSemaphore.release();
            return;
        }

        frameQueue.enqueue(frameData);
    }

    Q_Q(QnMjpegSession);
    emit q->frameEnqueued();
}

bool QnMjpegSessionPrivate::dequeueFrame(QImage *image, qint64 *timestamp, int *presentationTime) {
    QnMutexLocker lock(&mutex);

    if (frameQueue.isEmpty())
        return false;

    QnMjpegSessionPrivate::FrameData frameData = frameQueue.dequeue();

    if (image)
        *image = frameData.image;

    if (timestamp)
        *timestamp = frameData.timestamp;

    if (presentationTime)
        *presentationTime = frameData.presentationTime;

    queueSemaphore.release();

    return true;
}

bool QnMjpegSessionPrivate::connect() 
{
    if (!url.isValid())
        return false;

    if (reply)
        return false;

    boundary.clear();
    httpBuffer.clear();
    frameQueue.clear();
    parserState = ParseHeaders;
    previousFrameTimestampUSec = 0;
    frameTimestampUSec = 0;
    previousFrameData.clear();

    setState(QnMjpegSession::Connecting);

    reply = networkAccessManager->get(QNetworkRequest(url));
    reply->ignoreSslErrors();

    QObject::connect(reply, &QNetworkReply::readyRead, this, &QnMjpegSessionPrivate::at_reply_readyRead);
    QObject::connect(reply, &QNetworkReply::finished, this, &QnMjpegSessionPrivate::at_reply_finished);
    QObject::connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), this, &QnMjpegSessionPrivate::at_reply_error);

    return true;
}

void QnMjpegSessionPrivate::disconnect() 
{
    if (reply) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }

    boundary.clear();
    httpBuffer.clear();
    frameQueue.clear();
    previousFrameData.clear();

    setState(QnMjpegSession::Stopped);
}

void QnMjpegSessionPrivate::at_reply_readyRead() {
    /* We should process replies from the current and existing QNetworkReply object only. */
    if (reply != sender())
        return;

    if (boundary.isNull()) {
        if (!processHeaders())
            return;

        setState(QnMjpegSession::Playing);
    }

    while (state == QnMjpegSession::Playing) {
        if (!reply)
            break;

        qint64 available = reply->bytesAvailable();

        if (available < 1)
            break;

        if (httpBuffer.size() >= maxHttpBufferSize) {
            /* It means buffer wasn't processed and length limit is exceeded. */
            return;
        }

        int bytesToRead = qMin(available, maxHttpBufferSize - httpBuffer.size());
        int pos = httpBuffer.size();
        httpBuffer.resize(httpBuffer.size() + bytesToRead);
        qint64 read = reply->read(httpBuffer.data() + pos, bytesToRead);
        if (read < 0)
            return;

        if (read < bytesToRead)
            httpBuffer.truncate(pos + read);

        if (!processBuffer())
            break;
    }
}

void QnMjpegSessionPrivate::at_reply_error() {
    if (reply != sender())
        return;

    reply->deleteLater();
    reply = nullptr;

    if (state == QnMjpegSession::Connecting || QnMjpegSession::Playing)
        connect();
}

void QnMjpegSessionPrivate::at_reply_finished() {
    if (reply != sender())
        return;

    reply->deleteLater();
    reply = nullptr;

    if (state == QnMjpegSession::Connecting)
        connect();

    Q_Q(QnMjpegSession);
    q->finished();
}

bool QnMjpegSessionPrivate::processHeaders() {
    boundary.clear();

    QByteArray contentTypeHeader = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();

    QByteArray mimeType;

    int i = contentTypeHeader.indexOf(';');
    if (i > 0)
        mimeType = contentTypeHeader.left(i).trimmed();

    if (mimeType != "multipart/x-mixed-replace")
        return false;

    static const QByteArray boundaryMarker("boundary=");
    i = contentTypeHeader.indexOf(boundaryMarker, i);
    if (i > 0)
        boundary = contentTypeHeader.mid(i + boundaryMarker.size());

    if (boundary.isNull())
        return false;

    bodyLength = 0;

    return true;
}

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::processBuffer() {
    ParseResult result = ParseOk;

    if (processing)
        return result;

    processing = true;

    while (result == ParseOk && state == QnMjpegSession::Playing) {
        switch (parserState) {
        case ParseBoundary:
            result = parseBoundary();
            break;
        case ParseHeaders:
            result = parseHeaders();
            break;
        case ParseBody:
            result = parseBody();
            break;
        }
    }

    processing = false;

    return result;
}

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseBoundary() {
    int pos = httpBuffer.indexOf(boundary);
    if (pos < 0) {
        httpBuffer.remove(0, httpBuffer.size() - (boundary.size() - 1));
        return ParseNeedMoreData;
    }

    int boundaryStart = pos;

    pos += boundary.size();

    if (pos <= httpBuffer.size() - 2 && httpBuffer[pos] == '-' && httpBuffer[pos + 1] == '-')
        pos += 2;

    if (pos <= httpBuffer.size() - 1 && httpBuffer[pos] == '\n') {
        pos += 1;
    } else if (pos <= httpBuffer.size() - 2 && httpBuffer[pos] == '\r' && httpBuffer[pos + 1] == '\n') {
        pos += 2;
    } else if (pos > httpBuffer.size() - 2) {
        httpBuffer.remove(0, boundaryStart);
        return ParseNeedMoreData;
    } else {
        httpBuffer.remove(0, boundaryStart + boundary.size());
        return ParseNeedMoreData;
    }

    parserState = ParseHeaders;
    httpBuffer.remove(0, pos);
    return ParseOk;
}

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseHeaders() {
    int pos = 0;

    bool finalized = false;

    forever {
        int lineEnd = httpBuffer.indexOf('\n', pos);

        if (lineEnd < 0)
            break;

        /* Header length without \r\n */
        int length = ((lineEnd > 0 && httpBuffer[lineEnd - 1] == '\r') ? lineEnd - 1 : lineEnd) - pos;

        ++lineEnd;

        if (!finalized && length > 0) {
            static const QByteArray contentLengthMark("Content-Length:");
            static const QByteArray contentTypeMark("Content-Type:");
            static const QByteArray contentTimestampMark("x-Content-Timestamp:");

            if (length >= contentLengthMark.size() &&
                qstrnicmp(httpBuffer.data() + pos, contentLengthMark.data(), contentLengthMark.size()) == 0)
            {
                bodyLength = httpBuffer.mid(
                        pos + contentLengthMark.size(),
                        length - contentLengthMark.size()).trimmed().toLongLong();
            }
            else if (length >= contentTypeMark.size() &&
                     qstrnicmp(httpBuffer.data() + pos, contentTypeMark.data(), contentTypeMark.size()) == 0)
            {
                static const QByteArray tsMark("ts=");

                httpBuffer[lineEnd - 1] = '\0'; // workaround for missing strnstr function in msvc

                char *tsStr = strstr(httpBuffer.data() + pos + contentTypeMark.size(), tsMark);
                if (tsStr) {
                    tsStr += tsMark.size();
                    frameTimestampUSec = httpBuffer.mid(
                            tsStr - httpBuffer.data(),
                            httpBuffer.data() + pos + length - tsStr).trimmed().toLongLong();
                }

                httpBuffer[lineEnd - 1] = '\n';
            }
            else if (length >= contentTimestampMark.size() &&
                     qstrnicmp(httpBuffer.data() + pos, contentTimestampMark.data(), contentTimestampMark.size()) == 0)
            {
                frameTimestampUSec = httpBuffer.mid(
                        pos + contentTimestampMark.size(),
                        length - contentTimestampMark.size()).trimmed().toLongLong();
            }
        }

        pos = lineEnd;

        if (length == 0) {
            finalized = true;
            break;
        }
    }

    if (pos > 0)
        httpBuffer.remove(0, pos);

    if (finalized)
        parserState = ParseBody;

    return finalized ? ParseOk : ParseNeedMoreData;
}

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseBody() {
    if (bodyLength > 0) {
        if (httpBuffer.size() < bodyLength)
            return ParseNeedMoreData;
    } else {
        int pos = httpBuffer.indexOf(boundary);
        if (pos < 0)
            return ParseNeedMoreData;

        bodyLength = pos;
    }

    parserState = ParseBoundary;
    int framePresentationTimeMSec = previousFrameTimestampUSec > 0 ? (frameTimestampUSec - previousFrameTimestampUSec) / 1000 : 0;
    if (framePresentationTimeMSec > MAX_FRAME_DURATION)
        framePresentationTimeMSec = 0;

    if (!previousFrameData.isEmpty())
        decodeAndEnqueueFrame(previousFrameData, previousFrameTimestampUSec / 1000, framePresentationTimeMSec);

    if (finalTimestampMs > 0 && frameTimestampUSec > finalTimestampMs * 1000) {
        disconnect();
        Q_Q(QnMjpegSession);
        q->finished();
        return ParseOk;
    }

    previousFrameTimestampUSec = frameTimestampUSec;
    frameTimestampUSec = 0;

    previousFrameData = httpBuffer.left(bodyLength);

    httpBuffer.remove(0, bodyLength);

    return ParseOk;
}


QnMjpegSession::QnMjpegSession(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnMjpegSessionPrivate(this))
{
    Q_D(QnMjpegSession);

    d->networkAccessManager = new QNetworkAccessManager(this);
}

QnMjpegSession::~QnMjpegSession() {
}

QUrl QnMjpegSession::url() const {
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->url;
}

void QnMjpegSession::setUrl(const QUrl &url) {
    Q_D(QnMjpegSession);

    {
        QnMutexLocker lock(&d->mutex);

        if (d->url == url)
            return;

        d->url = url;
    }

    emit urlChanged();
}

QnMjpegSession::State QnMjpegSession::state() const {
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->state;
}

bool QnMjpegSession::dequeueFrame(QImage *image, qint64 *timestamp, int *presentationTime) {
    Q_D(QnMjpegSession);

    return d->dequeueFrame(image, timestamp, presentationTime);
}

qint64 QnMjpegSession::finalTimestampMs() const {
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->finalTimestampMs;
}

void QnMjpegSession::setFinalTimestampMs(qint64 finalTimestampMs) {
    Q_D(QnMjpegSession);

    {
        QnMutexLocker lock(&d->mutex);

        if (d->finalTimestampMs == finalTimestampMs)
            return;

        d->finalTimestampMs = finalTimestampMs;
    }

    emit finalTimestampChanged();
}

void QnMjpegSession::start() {
    Q_D(QnMjpegSession);

    executeDelayed([d]() { d->connect(); }, 0, thread());
}

void QnMjpegSession::stop() {
    Q_D(QnMjpegSession);

    {
        d->setState(Disconnecting);
        QnMutexLocker lock(&d->mutex);
        d->queueSemaphore.release(maxFrameQueueSize - d->queueSemaphore.available());
    }

    executeDelayed([d]() { d->disconnect(); }, 0, thread());
}
