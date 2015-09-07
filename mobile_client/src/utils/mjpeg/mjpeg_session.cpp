#include "mjpeg_session.h"

#include <QtCore/QUrl>
#include <QtCore/QQueue>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace {
    const qint64 maxHttpBufferSize = 2 * 1024 * 1024;
    const qint64 maxFrameQueueSize = 8;
}

class QnMjpegSessionPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegSession)
public:
    struct FrameData {
        int disaplyTime;
        QByteArray data;
    };

    QnMjpegSession *q_ptr;

    QUrl url;
    qint64 frameTimestamp;
    qint64 nextFrameTimestamp;
    QQueue<FrameData> frameQueue;

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

    void enqueueFrame(const QByteArray &data, int displayTime);

    void connect();
    void disconnect();

    void at_reply_readyRead();
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
    , networkAccessManager(nullptr)
    , reply(nullptr)
    , state(QnMjpegSession::Stopped)
    , processing(false)
    , parserState(ParseBoundary)
{}

void QnMjpegSessionPrivate::setState(QnMjpegSession::State state) {
    if (this->state == state)
        return;

    this->state = state;
}

void QnMjpegSessionPrivate::enqueueFrame(const QByteArray &data, int displayTime) {
    Q_Q(QnMjpegSession);

    FrameData frameData;
    frameData.data = data;
    frameData.disaplyTime = displayTime;

    frameQueue.enqueue(frameData);
    q->frameEnqueued();
}

void QnMjpegSessionPrivate::connect() {
    if (!url.isValid())
        return;

    if (reply)
        return;

    boundary.clear();
    httpBuffer.clear();
    frameQueue.clear();
    parserState = ParseHeaders;
    frameTimestamp = 0;
    nextFrameTimestamp = 0;

    setState(QnMjpegSession::Connecting);

    Q_Q(QnMjpegSession);

    if (!networkAccessManager)
        networkAccessManager = new QNetworkAccessManager(q);

    reply = networkAccessManager->get(QNetworkRequest(url));
    reply->ignoreSslErrors();

    QObject::connect(reply, &QNetworkReply::readyRead, this, &QnMjpegSessionPrivate::at_reply_readyRead);
    QObject::connect(reply, &QNetworkReply::finished, this, &QnMjpegSessionPrivate::at_reply_finished);
    QObject::connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), this, &QnMjpegSessionPrivate::at_reply_finished);
}

void QnMjpegSessionPrivate::disconnect() {
    if (reply) {
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }

    boundary.clear();
    httpBuffer.clear();
    frameQueue.clear();

    setState(QnMjpegSession::Stopped);
}

void QnMjpegSessionPrivate::at_reply_readyRead() {
    if (!reply)
        return;

    if (boundary.isNull()) {
        if (!processHeaders())
            return;

        setState(QnMjpegSession::Playing);
    }

    while (state == QnMjpegSession::Playing && frameQueue.size() < maxFrameQueueSize) {
        if (!reply)
            break;

        qint64 available = reply->bytesAvailable();

        if (available < 1)
            break;

        if (httpBuffer.size() >= maxHttpBufferSize)
            return;

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

void QnMjpegSessionPrivate::at_reply_finished() {
    if (!reply)
        return;

    reply->deleteLater();
    reply = nullptr;

    if (state == QnMjpegSession::Connecting || QnMjpegSession::Playing)
        connect();
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

    const char boundaryMarker[] = "boundary=";
    i = contentTypeHeader.indexOf(boundaryMarker, i);
    if (i > 0)
        boundary = contentTypeHeader.mid(i + sizeof(boundaryMarker) - 1);

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

    while (result == ParseOk && frameQueue.size() < maxFrameQueueSize) {
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
        httpBuffer.remove(0, httpBuffer.size() - 2);
        return ParseNeedMoreData;
    }

    int boundaryStart = pos;

    pos += boundary.size();
    int rest = httpBuffer.size() - pos;

    if (rest >= 2 && httpBuffer[pos] == '-' && httpBuffer[pos + 1] == '-') {
        pos += 2;
        rest -= 2;
    }

    if (rest >= 1 && httpBuffer[pos] == '\n') {
        pos += 1;
    } else if (rest >= 2 && httpBuffer[pos] == '\r' && httpBuffer[pos + 1] == '\n') {
        pos += 2;
    } else if (rest < 2) {
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

        unsigned length = ((lineEnd > 0 && httpBuffer[lineEnd - 1] == '\r') ? lineEnd - 1 : lineEnd) - pos;

        ++lineEnd;

        if (!finalized && length > 0) {
            static const char contentLengthMark[] = "Content-Length:";
            static const char contentTypeMark[] = "Content-Type:";
            static const char contentTimestampMark[] = "x-Content-Timestamp:";

            if (length >= sizeof(contentLengthMark) && qstrnicmp(httpBuffer.data() + pos, contentLengthMark, sizeof(contentLengthMark) - 1) == 0) {
                bodyLength = httpBuffer.mid(pos + sizeof(contentLengthMark) - 1,
                                            length - sizeof(contentLengthMark) + 1).trimmed().toLongLong();
            } else if (length >= sizeof(contentTypeMark) && qstrnicmp(httpBuffer.data() + pos, contentTypeMark, sizeof(contentTypeMark) - 1) == 0) {
                static const char tsMark[] = "ts=";

                httpBuffer[lineEnd - 1] = '\0'; // workaround for missing strnstr function in msvc

                char *tsStr = strstr(httpBuffer.data() + pos + sizeof(contentTypeMark), tsMark);
                if (tsStr) {
                    tsStr += sizeof(tsMark) - 1;
                    nextFrameTimestamp = httpBuffer.mid(tsStr - httpBuffer.data(), (httpBuffer.data() + lineEnd - 1) - tsStr).trimmed().toLongLong();
                }

                httpBuffer[lineEnd - 1] = '\n';
            } else if (length >= sizeof(contentTimestampMark) && qstrnicmp(httpBuffer.data() + pos, contentTimestampMark, sizeof(contentTimestampMark) - 1) == 0) {
                nextFrameTimestamp = httpBuffer.mid(pos + sizeof(contentTimestampMark) - 1,
                                                length - sizeof(contentTimestampMark) + 1).trimmed().toLongLong();
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
    int frameDisplayTime = frameTimestamp > 0 ? (nextFrameTimestamp - frameTimestamp) / 1000 : 0;
    frameTimestamp = nextFrameTimestamp;
    nextFrameTimestamp = 0;

    enqueueFrame(httpBuffer.left(bodyLength), frameDisplayTime);
    httpBuffer.remove(0, bodyLength);

    return ParseOk;
}


QnMjpegSession::QnMjpegSession(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnMjpegSessionPrivate(this))
{
}

QnMjpegSession::~QnMjpegSession() {
}

QUrl QnMjpegSession::url() const {
    Q_D(const QnMjpegSession);
    return d->url;
}

void QnMjpegSession::setUrl(const QUrl &url) {
    Q_D(QnMjpegSession);

    if (d->url == url)
        return;

    d->url = url;

    bool playing = state() == Playing;

    d->disconnect();

    if (playing)
        d->connect();

    emit urlChanged();
}

QnMjpegSession::State QnMjpegSession::state() const {
    Q_D(const QnMjpegSession);
    return d->state;
}

bool QnMjpegSession::dequeueFrame(QByteArray *data, int *displayTime) {
    Q_D(QnMjpegSession);

    if (d->frameQueue.isEmpty())
        return false;

    QnMjpegSessionPrivate::FrameData frameData = d->frameQueue.dequeue();

    if (data)
        *data = frameData.data;

    if (displayTime)
        *displayTime = frameData.disaplyTime;

    if (d->frameQueue.size() == maxFrameQueueSize - 1)
        d->processBuffer();

    return true;
}

void QnMjpegSession::play() {
    Q_D(QnMjpegSession);

    if (state() == Paused) {
        d->setState(Playing);
        d->processBuffer();
    } else {
        d->connect();
    }
}

void QnMjpegSession::stop() {
    Q_D(QnMjpegSession);

    d->disconnect();
}

void QnMjpegSession::pause() {
    Q_D(QnMjpegSession);

    if (state() == Playing)
        d->setState(Paused);
}
