#ifndef __TCP_CONNECTION_PRIV_H__
#define __TCP_CONNECTION_PRIV_H__

#include <QDateTime>
#include <QtCore/QByteArray>

#include "tcp_connection_processor.h"

#include "utils/common/byte_array.h"
#include <nx/network/http/http_types.h>
#include <nx/network/http/http_stream_reader.h>
#include <core/resource_access/user_access_data.h>


static const int TCP_READ_BUFFER_SIZE = 65536;

static const QByteArray STATIC_UNAUTHORIZED_HTML(
    "<!DOCTYPE html><HTML><BODY><H1>401 Unauthorized.</H1></BODY></HTML>"
);

static const QByteArray STATIC_FORBIDDEN_HTML(
    "<!DOCTYPE html><HTML><BODY><H1>403 Forbidden.</H1></BODY></HTML>"
);

static const QByteArray STATIC_PROXY_UNAUTHORIZED_HTML(
    "<!DOCTYPE html><HTML><BODY><H1>407 Proxy Unauthorized.</H1></BODY></HTML>"
);


// TODO: #vasilenko these are part of a public interface and are used throughout the codebase.
// Why they are in a private header???
static const int CODE_OK = 200;
static const int CODE_MOVED_PERMANENTLY = 301;
static const int CODE_NOT_MODIFIED = 304;
static const int CODE_BAD_REQUEST = 400;
static const int CODE_AUTH_REQUIRED = 401;
static const int CODE_NOT_FOUND = 404;
static const int CODE_PROXY_AUTH_REQUIRED = 407;
static const int CODE_INVALID_PARAMETER = 451;
static const int CODE_UNSPOORTED_TRANSPORT = 461;
static const int CODE_NOT_IMPLEMETED = 501;
static const int CODE_INTERNAL_ERROR = 500;

class QnTCPConnectionProcessorPrivate
{
    friend class QnTCPConnectionProcessor;

public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTCPConnectionProcessorPrivate():
        tcpReadBuffer(new quint8[TCP_READ_BUFFER_SIZE]),
        socketTimeout(5*1000),
        chunkedMode(false),
        clientRequestOffset(0),
        prevSocketError(SystemError::noError),
        authenticatedOnce(false),
        owner(nullptr),
        isSocketTaken(false),
        interleavedMessageDataPos(0),
        currentRequestSize(0)
    {
    }

    virtual ~QnTCPConnectionProcessorPrivate()
    {
        delete [] tcpReadBuffer;
    }

public:
    QSharedPointer<AbstractStreamSocket> socket;
    nx::network::http::Request request;
    nx::network::http::Response response;
    nx::network::http::HttpStreamReader httpStreamReader;

    QByteArray protocol;
    QByteArray requestBody;
    //QByteArray responseBody;
    QByteArray clientRequest;
    QByteArray receiveBuffer;
    QnMutex sockMutex;
    quint8* tcpReadBuffer;
    int socketTimeout;
    bool chunkedMode;
    int clientRequestOffset;
    QDateTime lastModified;
    Qn::UserAccessData accessRights;
    SystemError::ErrorCode prevSocketError;
    bool authenticatedOnce;
    QnTcpListener* owner;
    bool isSocketTaken;

private:
    QByteArray interleavedMessageData;
    size_t interleavedMessageDataPos;
    size_t currentRequestSize;
};

#endif // __TCP_CONNECTION_PRIV_H__
