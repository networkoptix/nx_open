// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QDateTime>
#include <chrono>
#include <optional>

#include <QtCore/QByteArray>

#include <nx/kit/utils.h>
#include <nx/network/http/http_stream_reader.h>
#include <nx/network/http/http_types.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "tcp_connection_processor.h"

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
        requestLogged(false),
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

        const auto beforeSendTime = steady_clock::now();
        SendDataResult result;
        result.sendResult = socket->send(data, size);
        const auto afterSendTime = steady_clock::now();

        if (result.sendResult < 0)
            result.errorCode = SystemError::getLastOSErrorCode();

        const int64_t sendDurationUs =
            duration_cast<microseconds>(afterSendTime - beforeSendTime).count();

        const auto logLevel =
            (sendDurationUs >= nx::network::ini().minSocketSendDurationUs)
                ? nx::log::Level::warning
                : nx::log::Level::verbose;

        NX_UTILS_LOG(logLevel, this,
            "Called send(@%1, %2 bytes) -> %3, timestamp %4, duration %5 us, since last send %6%7",
            nx::kit::utils::toString((const void*) data),
            size,
            (
                ((result.sendResult == size) ? "ok" : QString::number(result.sendResult))
                +
                ((result.sendResult >= 0) ? "" : nx::format(" (OS code %1)").args((int) result.errorCode))
            ),
            timestampForLogging ? nx::format("%1 us").args(*timestampForLogging) : "n/a",
            duration_cast<microseconds>(afterSendTime - beforeSendTime).count(),
            lastSendTime
                ? nx::format("%1 us").args(duration_cast<microseconds>(
                    beforeSendTime - *lastSendTime).count())
                : "n/a",
            (result.sendResult < 0)
                ? ("; OS error text: " + SystemError::toString(result.errorCode))
                : ""
        );

        lastSendTime = beforeSendTime;
        return result;
    }

public:
    std::unique_ptr<nx::network::AbstractStreamSocket> socket;
    nx::network::http::Request request;
    nx::network::http::Response response;
    nx::network::http::HttpStreamReader httpStreamReader;

    nx::String protocol;
    QByteArray requestBody;
    //QByteArray responseBody;
    QByteArray clientRequest;
    QByteArray receiveBuffer;
    quint8* tcpReadBuffer;
    bool chunkedMode;
    bool binaryProtocol = false;
    int clientRequestOffset;
    QDateTime lastModified;
    nx::network::rest::UserAccessData accessRights;
    SystemError::ErrorCode prevSocketError;
    bool authenticatedOnce;
    bool requestLogged;
    QnTcpListener* owner;
    mutable nx::Mutex socketMutex;
    // TODO: #rvasilenko Fix socketMutex used for takeSocket() vs sockMutex used for send().

private:
    nx::Mutex sendDataMutex;
    QByteArray interleavedMessageData;
    size_t interleavedMessageDataPos;
    qint64 currentRequestSize;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> lastSendTime;
};
