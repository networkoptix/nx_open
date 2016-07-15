#pragma once
#include "modbus.h"
#include <utils/network/system_socket.h>

namespace nx_modbus
{

class QnModbusAsyncClient: public QObject
{
    Q_OBJECT

    enum class ModbusClientState
    {
        ready,
        sendingMessage,
        readingHeader,
        readingData
    };

public:
    QnModbusAsyncClient();
    QnModbusAsyncClient(const SocketAddress& endpoint);

    void setEndpoint(const SocketAddress& endpoint);

    void doModbusRequestAsync(const ModbusRequest& request);

    void readDiscreteInputsAsync();

    void readCoilsAsync(quint16 startCoil, quint16 coilNumber);
    void writeCoilsAsync();

    void readInputRegistersAsync();

    void readHoldingRegistersAsync(quint32 startRegister, quint32 registerCount);
    void writeHoldingRegistersAsync(quint32 startRegister, const QByteArray& data);

    QString getLastErrorString() const;

signals:
    void done(ModbusResponse response);
    void error();

private:
    void initSocket();
    void readAsync();
    void asyncSendDone(AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesWritten);
    void onSomeBytesReadAsync(AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead);

    void processState();
    void processHeader();
    void processData();

    ModbusMBAPHeader buildHeader(const ModbusRequest& request);

private:
    quint16 m_requestTransactionId;
    quint16 m_requestFunctionCode;
    quint16 m_responseLength;

    QString m_lastErrorString;

    SocketAddress m_endpoint;

    ModbusClientState m_state;
    QByteArray m_recvBuffer;
    QByteArray m_sendBuffer;
    std::shared_ptr<AbstractStreamSocket> m_socket;
};

}


