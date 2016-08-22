#pragma once

#include "modbus.h"
#include "modbus_message_parser.h"
#include "modbus_message_serializer.h"

#include <nx/network/system_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>


using namespace nx::network;

using ModbusProtocolConnection = 
    nx_api::BaseStreamProtocolConnectionEmbeddable<
        nx_modbus::ModbusMessage,
        nx_modbus::ModbusMessageParser,
        nx_modbus::ModbusMessageSerializer>;

namespace nx_modbus
{

class QnModbusAsyncClient: 
    public QObject,
    public StreamConnectionHolder<ModbusProtocolConnection>,
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
    QnModbusAsyncClient(const SocketAddress& endpoint);

    QnModbusAsyncClient(const QnModbusAsyncClient& other) = delete;
    QnModbusAsyncClient &operator=(const QnModbusAsyncClient& other) = delete;

    ~QnModbusAsyncClient();

    void setEndpoint(const SocketAddress& endpoint);

    void doModbusRequestAsync(const ModbusMessage& request);

    quint16 readDiscreteInputsAsync(quint16 startRegister, quint16 registerCount);

    quint16 readCoilsAsync(quint16 startCoil, quint16 coilNumber);
    quint16 writeCoilsAsync(quint16 startCoilAddress, const QByteArray& data);

    quint16 readInputRegistersAsync(quint16 startRegister, quint16 registerCount);

    quint16 readHoldingRegistersAsync(quint32 startRegister, quint32 registerCount);
    quint16 writeHoldingRegistersAsync(quint32 startRegister, const QByteArray& data);

    QString getLastErrorString() const;

    // Impl of StreamConnectionHolder::closeConnection
    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override;
    
    // Impl of BasicPollable::stopWhileInAioThread
    virtual void stopWhileInAioThread() override;

signals:
    void done(ModbusMessage response);
    void error();

private:
    void openConnection();
    void onConnectionDone(SystemError::ErrorCode errorCode);

    void sendPendingMessage();
    void onMessage(ModbusMessage message);
    void onError(SystemError::ErrorCode errorCode, QString& errorStr);

    ModbusMBAPHeader buildHeader(const ModbusMessage& request);

private:
    ModbusClientState m_state;
    std::unique_ptr<ModbusProtocolConnection> m_modbusConnection;
    quint16 m_requestSequenceNumber;
    ModbusMessage m_pendingMessage;
    bool m_hasPendingMessage;
    SocketAddress m_endpoint;
    QString m_lastErrorString;
    mutable QnMutex m_mutex; 
};

}


