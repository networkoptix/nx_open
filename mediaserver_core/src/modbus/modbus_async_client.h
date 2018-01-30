#pragma once

#include "modbus.h"
#include "modbus_message_parser.h"
#include "modbus_message_serializer.h"

#include <nx/network/system_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>

namespace nx
{
namespace modbus
{

using ModbusProtocolConnection =
    nx::network::server::BaseStreamProtocolConnectionEmbeddable<
        ModbusMessage,
        ModbusMessageParser,
        ModbusMessageSerializer>;

using namespace nx::network;

class QnModbusAsyncClient: 
    public QObject,
    public network::server::StreamConnectionHolder<ModbusProtocolConnection>,
    public aio::BasicPollable
{
    Q_OBJECT

    enum class ModbusClientState
    {
        disconnected,
        connecting,
        connected
    };

public:
    QnModbusAsyncClient();
    QnModbusAsyncClient(const nx::network::SocketAddress& endpoint);

    QnModbusAsyncClient(const QnModbusAsyncClient& other) = delete;
    QnModbusAsyncClient &operator=(const QnModbusAsyncClient& other) = delete;

    ~QnModbusAsyncClient();

    void setEndpoint(const nx::network::SocketAddress& endpoint);

    void doModbusRequestAsync(ModbusMessage request);

    void readDiscreteInputsAsync(quint16 startRegister, quint16 registerCount, quint16* outTransactionId);

    void readCoilsAsync(quint16 startCoil, quint16 coilNumber, quint16* outTransactionId);
    void writeCoilsAsync(quint16 startCoilAddress, const QByteArray& data, quint16* outTransactionId);

    void readInputRegistersAsync(quint16 startRegister, quint16 registerCount, quint16* outTransactionId);

    void readHoldingRegistersAsync(quint32 startRegister, quint32 registerCount, quint16* outTransactionId);
    void writeHoldingRegistersAsync(quint32 startRegister, const QByteArray& data, quint16* outTransactionId);

    QString getLastErrorString() const;

    // Impl of StreamConnectionHolder::closeConnection
    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override;
    
    // Impl of BasicPollable::stopWhileInAioThread
    virtual void stopWhileInAioThread() override;

signals:
    // Only Qt::DirectConnection can be used to connect to these signals
    // Otherwise, client can be in an unidentified state upon signal delivery
    void done(ModbusMessage response);
    void error();

private:
    void openConnection();
    void onConnectionDone(SystemError::ErrorCode errorCode);

    void sendPendingMessage();
    void onMessage(ModbusMessage message);
    void onError(SystemError::ErrorCode errorCode, const QString& errorStr);

    void addHeader(ModbusMessage* request);

    ModbusMessage buildReadRequest(
        quint8 functionCode,
        quint16 startAddress,
        quint16 count);

    ModbusMessage buildWriteMultipleRequest(
        quint8 functionCode,
        quint16 startAddress,
        quint16 unitCount,
        quint8 byteCount,
        const QByteArray& data);

private:
    ModbusClientState m_state;
    std::unique_ptr<ModbusProtocolConnection> m_modbusConnection;
    quint16 m_requestSequenceNumber;
    ModbusMessage m_pendingMessage;
    bool m_hasPendingMessage;
    nx::network::SocketAddress m_endpoint;
    QString m_lastErrorString;
    mutable QnMutex m_mutex; 
};

} //< close namespace modbus

} //< close namespace nx


