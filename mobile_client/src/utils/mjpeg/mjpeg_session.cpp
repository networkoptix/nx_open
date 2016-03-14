#include "mjpeg_session.h"

#include <QtCore/QUrl>
#include <QtCore/QQueue>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/media/jpeg.h>

namespace {

const qint64 kMaxHttpBufferSize = 2 * 1024 * 1024;

} // namespace


QnMjpegSession::FrameData::FrameData()
:
    presentationTime(0),
    timestamp(0)
{
}

bool QnMjpegSession::FrameData::isNull() const
{
    return image.isNull();
}

void QnMjpegSession::FrameData::clear()
{
    image = QImage();
}


class QnMjpegSessionPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnMjpegSession)
public:
    QnMjpegSession *q_ptr;

    mutable QnMutex mutex;

    QUrl url;
    QByteArray previousFrameData;
    qint64 previousFrameTimestampUSec;
    qint64 frameTimestampUSec;
    QnMjpegSession::FrameData lastFrame;
    QnWaitCondition lastFrameReleased;

    qint64 finalTimestampMs;

    QNetworkAccessManager *networkAccessManager;
    QNetworkReply *reply;

    QnMjpegSession::State state;

    QByteArray boundary;
    QByteArray httpBuffer;
    qint64 bodyLength;

    bool processing;

    enum ParserState
	{
        ParseBoundary,
        ParseHeaders,
        ParseBody,
    };

    enum ParseResult
	{
        ParseOk,
        ParseNeedMoreData,
        ParseFailed,
    };

    ParserState parserState;

    QnMjpegSessionPrivate(QnMjpegSession *parent);

    void setState(QnMjpegSession::State state);

    void decodeFrame(const QByteArray &data, qint64 timestampMs, int presentationTime);
    QnMjpegSession::FrameData getFrame();

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
:
	QObject(parent),
    q_ptr(parent),
    finalTimestampMs(-1),
    networkAccessManager(nullptr),
    reply(nullptr),
    state(QnMjpegSession::Stopped),
    processing(false),
    parserState(ParseBoundary)
{
}

void QnMjpegSessionPrivate::setState(QnMjpegSession::State state)
{
    {
        QnMutexLocker lock(&mutex);

        if (this->state == state)
            return;

        this->state = state;
    }

    Q_Q(QnMjpegSession);
    emit q->stateChanged();
}

void QnMjpegSessionPrivate::decodeFrame(
	const QByteArray &data, qint64 timestampMs, int presentationTime)
{
    if (state != QnMjpegSession::Playing)
        return;

    QnMjpegSession::FrameData frameData;
    frameData.image = decompressJpegImage(data.data(), data.size());
    if (frameData.image.isNull())
        return;
    frameData.timestamp = timestampMs;
    frameData.presentationTime = presentationTime;

    {
        QnMutexLocker lock(&mutex);
        if (!lastFrame.isNull())
            lastFrameReleased.wait(&mutex);

        if (state != QnMjpegSession::Playing)
            return;

        lastFrame = frameData;
    }

    Q_Q(QnMjpegSession);
    emit q->frameAvailable();
}

QnMjpegSession::FrameData QnMjpegSessionPrivate::getFrame()
{
    QnMutexLocker lock(&mutex);

    QnMjpegSession::FrameData result;

    if (lastFrame.isNull())
        return result;

    result = std::move(lastFrame);

    lastFrameReleased.wakeAll();

    return result;
}

bool QnMjpegSessionPrivate::connect()
{
    if (!url.isValid())
        return false;

    if (reply)
        return false;

    boundary.clear();
    httpBuffer.clear();
    lastFrame.clear();
    parserState = ParseHeaders;
    previousFrameTimestampUSec = 0;
    frameTimestampUSec = 0;
    previousFrameData.clear();

    setState(QnMjpegSession::Connecting);

    reply = networkAccessManager->get(QNetworkRequest(url));
    reply->ignoreSslErrors();

    QObject::connect(
		reply, &QNetworkReply::readyRead, this, &QnMjpegSessionPrivate::at_reply_readyRead);
    QObject::connect(
		reply, &QNetworkReply::finished, this, &QnMjpegSessionPrivate::at_reply_finished);
    QObject::connect(
		reply,
		static_cast<void (QNetworkReply::*) (QNetworkReply::NetworkError)>(&QNetworkReply::error),
		this,
		&QnMjpegSessionPrivate::at_reply_error);

    return true;
}

void QnMjpegSessionPrivate::disconnect()
{
    if (reply)
	{
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }

    boundary.clear();
    httpBuffer.clear();
    lastFrame.clear();
    previousFrameData.clear();

    setState(QnMjpegSession::Stopped);
}

void QnMjpegSessionPrivate::at_reply_readyRead()
{
    // We should process replies from the current and existing QNetworkReply object only.
    if (reply != sender())
        return;

    if (boundary.isNull())
	{
        if (!processHeaders())
            return;

        setState(QnMjpegSession::Playing);
    }

    while (state == QnMjpegSession::Playing)
	{
        if (!reply)
            break;

        qint64 available = reply->bytesAvailable();

        if (available < 1)
            break;

        if (httpBuffer.size() >= kMaxHttpBufferSize)
		{
            // It means buffer wasn't processed and length limit is exceeded.
            return;
        }

        int bytesToRead = qMin(available, kMaxHttpBufferSize - httpBuffer.size());
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

void QnMjpegSessionPrivate::at_reply_error()
{
    if (reply != sender())
        return;

    reply->deleteLater();
    reply = nullptr;

    if (state == QnMjpegSession::Connecting || QnMjpegSession::Playing)
        connect();
}

void QnMjpegSessionPrivate::at_reply_finished()
{
    if (reply != sender())
        return;

    reply->deleteLater();
    reply = nullptr;

    if (state == QnMjpegSession::Connecting)
        connect();

    Q_Q(QnMjpegSession);
    q->finished();
}

bool QnMjpegSessionPrivate::processHeaders()
{
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

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::processBuffer()
{
    ParseResult result = ParseOk;

    if (processing)
        return result;

    processing = true;

    while (result == ParseOk && state == QnMjpegSession::Playing)
	{
        switch (parserState)
		{
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

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseBoundary()
{
    int pos = httpBuffer.indexOf(boundary);
    if (pos < 0)
	{
        httpBuffer.remove(0, httpBuffer.size() - (boundary.size() - 1));
        return ParseNeedMoreData;
    }

    int boundaryStart = pos;

    pos += boundary.size();

    if (pos <= httpBuffer.size() - 2 && httpBuffer[pos] == '-' && httpBuffer[pos + 1] == '-')
        pos += 2;

    if (pos <= httpBuffer.size() - 1 && httpBuffer[pos] == '\n')
	{
        pos += 1;
    }
	else if (pos <= httpBuffer.size() - 2 && httpBuffer[pos] == '\r' && httpBuffer[pos + 1] == '\n')
	{
        pos += 2;
    }
	else if (pos > httpBuffer.size() - 2)
	{
        httpBuffer.remove(0, boundaryStart);
        return ParseNeedMoreData;
    }
	else
	{
        httpBuffer.remove(0, boundaryStart + boundary.size());
        return ParseNeedMoreData;
    }

    parserState = ParseHeaders;
    httpBuffer.remove(0, pos);
    return ParseOk;
}

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseHeaders()
{
    int pos = 0;

    bool finalized = false;

	for (;;)
	{
        int lineEnd = httpBuffer.indexOf('\n', pos);

        if (lineEnd < 0)
            break;

        // Header length without "\r\n".
        int length =
			((lineEnd > 0 && httpBuffer[lineEnd - 1] == '\r') ? lineEnd - 1 : lineEnd) - pos;

        ++lineEnd;

        if (!finalized && length > 0)
		{
            static const QByteArray contentLengthMark("Content-Length:");
            static const QByteArray contentTypeMark("Content-Type:");
            static const QByteArray contentTimestampMark("x-Content-Timestamp:");

            if (length >= contentLengthMark.size() && qstrnicmp(
				httpBuffer.data() + pos, contentLengthMark.data(), contentLengthMark.size()) == 0)
            {
                bodyLength = httpBuffer.mid(
                        pos + contentLengthMark.size(),
                        length - contentLengthMark.size()).trimmed().toLongLong();
            }
            else if (length >= contentTypeMark.size() && qstrnicmp(
				httpBuffer.data() + pos, contentTypeMark.data(), contentTypeMark.size()) == 0)
            {
                static const QByteArray tsMark("ts=");

                httpBuffer[lineEnd - 1] = '\0'; //< workaround for missing strnstr function in msvc

                char *tsStr = strstr(httpBuffer.data() + pos + contentTypeMark.size(), tsMark);
                if (tsStr)
				{
                    tsStr += tsMark.size();
                    frameTimestampUSec = httpBuffer.mid(
                        tsStr - httpBuffer.data(),
                        httpBuffer.data() + pos + length - tsStr).trimmed().toLongLong();
                }

                httpBuffer[lineEnd - 1] = '\n';
            }
            else if (length >= contentTimestampMark.size() && qstrnicmp(
				httpBuffer.data() + pos, contentTimestampMark.data(), contentTimestampMark.size()) == 0)
            {
                frameTimestampUSec = httpBuffer.mid(
                    pos + contentTimestampMark.size(),
                    length - contentTimestampMark.size()).trimmed().toLongLong();
            }
        }

        pos = lineEnd;

        if (length == 0)
		{
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

QnMjpegSessionPrivate::ParseResult QnMjpegSessionPrivate::parseBody()
{
    if (bodyLength > 0)
	{
        if (httpBuffer.size() < bodyLength)
            return ParseNeedMoreData;
    }
	else
	{
        int pos = httpBuffer.indexOf(boundary);
        if (pos < 0)
            return ParseNeedMoreData;

        bodyLength = pos;
    }

    parserState = ParseBoundary;
    int framePresentationTimeMSec = (previousFrameTimestampUSec > 0) ?
		(frameTimestampUSec - previousFrameTimestampUSec) / 1000 : 0;
    framePresentationTimeMSec = qBound(0, framePresentationTimeMSec, static_cast<int>(MAX_FRAME_DURATION));

    if (!previousFrameData.isEmpty())
        decodeFrame(previousFrameData, previousFrameTimestampUSec / 1000, framePresentationTimeMSec);

    if (finalTimestampMs > 0 && frameTimestampUSec > finalTimestampMs * 1000)
	{
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
:
	QObject(parent),
    d_ptr(new QnMjpegSessionPrivate(this))
{
    Q_D(QnMjpegSession);

    d->networkAccessManager = new QNetworkAccessManager(this);
}

QnMjpegSession::~QnMjpegSession()
{
}

QUrl QnMjpegSession::url() const
{
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->url;
}

void QnMjpegSession::setUrl(const QUrl &url)
{
    Q_D(QnMjpegSession);

    {
        QnMutexLocker lock(&d->mutex);

        if (d->url == url)
            return;

        d->url = url;
    }

    emit urlChanged();
}

QnMjpegSession::State QnMjpegSession::state() const
{
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->state;
}

QnMjpegSession::FrameData QnMjpegSession::getFrame()
{
    Q_D(QnMjpegSession);

    return d->getFrame();
}

qint64 QnMjpegSession::finalTimestampMs() const
{
    Q_D(const QnMjpegSession);

    QnMutexLocker lock(&d->mutex);

    return d->finalTimestampMs;
}

void QnMjpegSession::setFinalTimestampMs(qint64 finalTimestampMs)
{
    Q_D(QnMjpegSession);

    {
        QnMutexLocker lock(&d->mutex);

        if (d->finalTimestampMs == finalTimestampMs)
            return;

        d->finalTimestampMs = finalTimestampMs;
    }

    emit finalTimestampChanged();
}

void QnMjpegSession::start()
{
    Q_D(QnMjpegSession);

    executeDelayed(
		[d]()
	    {
			d->connect();
		},
		0,
		thread());
}

void QnMjpegSession::stop()
{
    Q_D(QnMjpegSession);

    d->setState(Disconnecting);
    d->lastFrameReleased.wakeAll();

    executeDelayed(
		[d]()
		{
			d->disconnect();
		},
		0,
		thread());
}
