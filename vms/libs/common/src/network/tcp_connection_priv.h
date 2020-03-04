#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QtCore/QByteArray>

#include "tcp_connection_processor.h"

#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
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
        if (socket)
            socket->pleaseStopSync();
        delete [] tcpReadBuffer;
    }

    struct SendDataResult
    {
        /** Result of socket->send(). */
        int sendResult;

        /** Valid only if sendResult < 0. */
        SystemError::ErrorCode errorCode = SystemError::noError;
    };

    SendDataResult sendData(
        const char* data, int size, std::optional<int64_t> timestampForLogging = std::nullopt)
    {
        using namespace std::chrono;

        const auto beforeSend = steady_clock::now();
        SendDataResult result;
        result.sendResult = socket->send(data, size);
        const auto afterSend = steady_clock::now();

        if (result.sendResult < 0)
            result.errorCode = SystemError::getLastOSErrorCode();

        NX_VERBOSE(this,
            "Called send(@%1, %2 bytes) -> %3, timestamp %4, duration %5 us, since last send %6%7",
            nx::kit::utils::toString((const void*) data),
            size,
            (
                ((result.sendResult == size) ? "ok" : QString::number(result.sendResult))
                +
                ((result.sendResult >= 0) ? "" : lm(" (OS code %1)").args((int) result.errorCode))
            ),
            timestampForLogging ? lm("%1 us").args(*timestampForLogging) : "n/a",
            duration_cast<microseconds>(afterSend - beforeSend).count(),
            lastSendTime
                ? lm("%1 us").args(duration_cast<microseconds>(
                    beforeSend - *lastSendTime).count())
                : "n/a",
            (result.sendResult < 0)
                ? ("; OS error text: " + SystemError::toString(result.errorCode))
                : ""
        );

        lastSendTime = beforeSend;
        return result;
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
    quint8* tcpReadBuffer;
    bool chunkedMode;
    int clientRequestOffset;
    QDateTime lastModified;
    Qn::UserAccessData accessRights;
    SystemError::ErrorCode prevSocketError;
    bool authenticatedOnce;
    QnTcpListener* owner;
    mutable QnMutex socketMutex;
    // TODO: #rvasilenko Fix socketMutex used for takeSocket() vs sockMutex used for send().

private:
    QnMutex sendDataMutex;
    QByteArray interleavedMessageData;
    size_t interleavedMessageDataPos;
    size_t currentRequestSize;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> lastSendTime;
};
