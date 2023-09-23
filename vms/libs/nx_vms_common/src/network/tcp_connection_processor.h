// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <api/model/audit/auth_session.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket.h>
#include <nx/network/socket_delegate.h>
#include <nx/string.h>
#include <nx/utils/byte_array.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context_aware.h>

class QnTcpListener;
class QnTCPConnectionProcessorPrivate;

static constexpr char kRfc4571Stun[] = "RFC4571+STUN";

namespace Qn { struct UserAccessData; }

class NX_VMS_COMMON_API QnTCPConnectionProcessor:
    public QnLongRunnable,
    public /*mixin*/ nx::vms::common::SystemContextAware
{
    using base_type = QnLongRunnable;

public:
    static const int KEEP_ALIVE_TIMEOUT = 5  * 1000;

    /**
     * Called from templates. In derived classes, redefine to return true if the last path
     * component carries camera id.
     */
    static bool doesPathEndWithCameraId() { return false; }

    QnTCPConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        int maxTcpRequestSize);

    virtual ~QnTCPConnectionProcessor();

    virtual void stop() override;

    /**
     * Check for request or response is completed: finished with /r/n/r/n or contains full content len data
     * \param outContentLen If Content-Length header found, saves its value to \a *outContentLen (if not null)
     * Returns -1 if message parsing fails, 0 if message is incomplete and message size if it is complete.
     */
    static int isFullMessage(
        const QByteArray& message,
        std::optional<qint64>* const fullMessageSize = nullptr );

    static int isFullMessage(
        const nx::Buffer& message,
        std::optional<qint64>* const fullMessageSize = nullptr );

    bool isLargeRequestAllowed(const nx::utils::Url& url);
    bool isLargeRequestAllowed(const QByteArray& message);

    /**
     * Check for some known binary protocol(s)
     * If yes, fill 'protocol' and 'size' field
     */
    int checkForBinaryProtocol(const QByteArray& message);
    bool isBinaryProtocol() const;

    bool sendChunk(const nx::utils::ByteArray& chunk);
    bool sendChunk(const QByteArray& chunk);
    bool sendChunk(const nx::Buffer& chunk);
    bool sendChunk(const char* data, int size);

    void execute(nx::Locker<nx::Mutex>& mutex);
    virtual void pleaseStop() override;
    nx::network::SocketAddress getForeignAddress() const;
    nx::utils::Url getDecodedUrl() const;

    bool sendBuffer(
        const nx::utils::ByteArray& sendBuffer, std::optional<int64_t> timestampForLogging = std::nullopt);

    bool sendBuffer(
        const QByteArray& sendBuffer, std::optional<int64_t> timestampForLogging = std::nullopt);

    bool sendBuffer(
        const char* data, int size, std::optional<int64_t> timestampForLogging = std::nullopt);

    enum ReadResult
    {
        ok,
        incomplete,
        error,
    };

    /*!
        \bug In case of interleaved requests, this method reads everything after first HTTP request as message body
    */
    ReadResult readRequest();
    /*!
        Reads single HTTP request. To be used when HTTP interleaving is required
        \note After return of this method there is already-parsed request in d->request.
            No need to call QnTCPConnectionProcessor::parseRequest
        \note \a d->clientRequest is not filled by this method!
    */
    bool readSingleRequest();
    virtual void parseRequest();

    bool isConnectionSecure() const;
    std::unique_ptr<nx::network::AbstractStreamSocket> takeSocket();
    //void releaseSocket();

    int redirectTo(const nx::network::http::Method& method, const QByteArray& page, QByteArray* contentType);
    int notFound(QByteArray& contentType);
    QnAuthSession authSession() const;

    QnTcpListener* owner() const;

protected:
    QnAuthSession authSession(const Qn::UserAccessData& accessRights) const;
    QString extractPath() const;
    static QString extractPath(const QString& fullUrl);

    std::pair<nx::String, nx::String> generateErrorResponse(
        nx::network::http::StatusCode::Value errorCode, const nx::String& errorDetails = {}) const;

    //nx::utils::ByteArray& getSendBuffer();
    //void bufferData(const char* data, int size);
    //void bufferData(const QByteArray& data) { bufferData(data.constData(), data.size()); }
    //void clearBuffer();

    nx::String createResponse(
        int httpStatusCode,
        const nx::String& contentType,
        const nx::String& contentEncoding,
        const QByteArray& multipartBoundary,
        bool isUndefinedContentLength = false);

    void sendResponse(
        int httpStatusCode,
        const nx::String& contentType,
        const nx::String& contentEncoding = {},
        const QByteArray& multipartBoundary = {},
        bool isUndefinedContentLength = false);

    void copyClientRequestTo(QnTCPConnectionProcessor& other);
    nx::network::SocketAddress remoteHostAddress() const;

    QnTCPConnectionProcessor(
        QnTCPConnectionProcessorPrivate* d_ptr,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        int maxTcpRequestSize);

    /** For inherited classes without a TCP server socket only. */
    QnTCPConnectionProcessor(
        QnTCPConnectionProcessorPrivate* dptr,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        nx::vms::common::SystemContext* systemContext,
        int maxTcpRequestSize);

    int maxTcpRequestSize() const { return m_maxTcpRequestSize; }

    void sendErrorResponse(
        nx::network::http::StatusCode::Value httpResult, const nx::String& errorDetails = {});

    void sendUnauthorizedResponse(
        nx::network::http::StatusCode::Value httpResult,
        const QByteArray& messageBody = {}, const nx::String& details = {});

    void logRequestOrResponse(
        const QByteArray& logMessage,
        const QByteArray& contentType,
        const QByteArray& contentEncoding,
        const QByteArray& httpMessageFull,
        const nx::Buffer& httpMessageBodyOnly);

protected:
    Q_DECLARE_PRIVATE(QnTCPConnectionProcessor);

    QnTCPConnectionProcessorPrivate *d_ptr;

    bool isConnectionCanBePersistent() const;

private:
    bool sendBufferThreadSafe(
        const char* data, int size, std::optional<int64_t> timestampForLogging);

private:
    int m_maxTcpRequestSize;
};
