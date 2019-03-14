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

class QnTCPConnectionProcessorPrivate
{
    friend class QnTCPConnectionProcessor;

public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTCPConnectionProcessorPrivate():
        tcpReadBuffer(new quint8[TCP_READ_BUFFER_SIZE]),
        chunkedMode(false),
        clientRequestOffset(0),
        prevSocketError(SystemError::noError),
        authenticatedOnce(false),
        owner(nullptr),
        interleavedMessageDataPos(0),
        currentRequestSize(0)
    {
    }

    virtual ~QnTCPConnectionProcessorPrivate()
    {
        delete [] tcpReadBuffer;
    }

public:
    std::unique_ptr<nx::network::AbstractStreamSocket> socket;
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
    bool chunkedMode;
    int clientRequestOffset;
    QDateTime lastModified;
    Qn::UserAccessData accessRights;
    SystemError::ErrorCode prevSocketError;
    bool authenticatedOnce;
    QnTcpListener* owner;
    mutable QnMutex socketMutex;
private:
    QByteArray interleavedMessageData;
    size_t interleavedMessageDataPos;
    size_t currentRequestSize;
};

#endif // __TCP_CONNECTION_PRIV_H__
