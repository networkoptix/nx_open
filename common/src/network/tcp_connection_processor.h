#pragma once

#include <QtCore/QUrl>

#include <common/common_module_aware.h>

#include "nx/utils/thread/long_runnable.h"
#include <nx/network/socket.h>
#include "utils/common/byte_array.h"
#include "api/model/audit/auth_session.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>

class QnTcpListener;
class QnTCPConnectionProcessorPrivate;

class QnTCPConnectionProcessor: public QnLongRunnable, public QnCommonModuleAware
{
    Q_OBJECT;

public:
    static const int KEEP_ALIVE_TIMEOUT = 5  * 1000;

    /**
     * Called from templates. In derived classes, redefine to return true if the last path
     * component carries camera id.
     */
    static bool doesPathEndWithCameraId() { return false; }

    QnTCPConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnTCPConnectionProcessor();

    /**
     * Check for request or response is completed: finished with /r/n/r/n or contains full content len data
     * \param outContentLen If Content-Length header found, saves its value to \a *outContentLen (if not null)
     * Returns -1 if message parsing fails, 0 if message is incomplete and message size if it is comlete.
     */
    static int isFullMessage(
        const QByteArray& message,
        boost::optional<qint64>* const fullMessageSize = nullptr );

    int getSocketTimeout();

    bool sendChunk(const QnByteArray& chunk);
    bool sendChunk(const QByteArray& chunk);
    bool sendChunk(const char* data, int size);

    void execute(QnMutex& mutex);
    virtual void pleaseStop();
    QSharedPointer<AbstractStreamSocket> socket() const;
    nx::utils::Url getDecodedUrl() const;

    bool sendBuffer(const QnByteArray& sendBuffer);
    bool sendBuffer(const QByteArray& sendBuffer);

    /*!
        \bug In case of interleaved requests, this method reads everything after first HTTP request as message body
    */
    bool readRequest();
    /*!
        Reads single HTTP request. To be used when HTTP interleaving is required
        \note After return of this method there is already-parsed request in d->request.
            No need to call QnTCPConnectionProcessor::parseRequest
        \note \a d->clientRequest is not filled by this method!
    */
    bool readSingleRequest();
    virtual void parseRequest();

    bool isSocketTaken() const;
    QSharedPointer<AbstractStreamSocket> takeSocket();
    void releaseSocket();

    int redirectTo(const QByteArray& page, QByteArray& contentType);
    int notFound(QByteArray& contentType);
    QnAuthSession authSession() const;

protected:
    QString extractPath() const;
    static QString extractPath(const QString& fullUrl);

    //QnByteArray& getSendBuffer();
    //void bufferData(const char* data, int size);
    //inline void bufferData(const QByteArray& data) { bufferData(data.constData(), data.size()); }
    //void clearBuffer();

    QByteArray createResponse(int httpStatusCode, const QByteArray& contentType,
        const QByteArray& contentEncoding, const QByteArray& multipartBoundary,
        bool displayDebug = false, bool isUndefinedContentLength = false);

    void sendResponse(int httpStatusCode, const QByteArray& contentType,
        const QByteArray& contentEncoding = {}, const QByteArray& multipartBoundary = {},
        bool displayDebug = false, bool isUndefinedContentLength = false);

    QString codeToMessage(int code);

    void copyClientRequestTo(QnTCPConnectionProcessor& other);
    /*!
        \return Number of bytes read. 0 if connection has been closed. -1 in case of error
        \note Usage of this method MUST NOT be mixed with usage of \a readRequest / \a parseRequest
    */
    int readSocket( quint8* buffer, int bufSize );
    SocketAddress remoteHostAddress() const;

    QnTCPConnectionProcessor(
        QnTCPConnectionProcessorPrivate* d_ptr,
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* owner);
    // For inherited classes without TCP server socket only
    QnTCPConnectionProcessor(
        QnTCPConnectionProcessorPrivate* dptr,
        QSharedPointer<AbstractStreamSocket> socket,
        QnCommonModule* commonModule);

    bool sendData(const char* data, int size);
    inline bool sendData(const QByteArray& data) { return sendData(data.constData(), data.size()); }
    void sendUnauthorizedResponse(nx_http::StatusCode::Value httpResult, const QByteArray& messageBody);
protected:
    Q_DECLARE_PRIVATE(QnTCPConnectionProcessor);

    QnTCPConnectionProcessorPrivate *d_ptr;

    bool isConnectionCanBePersistent() const;
};
