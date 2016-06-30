#pragma once
#include "modbus.h"
#include <utils/network/system_socket.h>

namespace nx_modbus
{

class QnModbusAsyncClient
{
    enum class ModbusClientState
    {
        Ready,
        SendingMessage,
        ReadingHeader,
        ReadingData
    };

public:
    QnModbusAsyncClient();
    void doModbusRequestAsync(const ModbusRequest& request);

    ModbusResponse readDiscreteInputsAsync();

    ModbusResponse readCoilsAsync();
    ModbusResponse writeCoilsAsync();

    ModbusResponse readInputRegistersAsync();

    ModbusResponse readHoldingRegistersAsync(quint16 startRegister, quint16 registerCount);
    ModbusResponse writeHoldingRegistersAsync(quint16 startRegister, const QByteArray& data);

signals:
    void done(const ModbusResponse& response);
    void error();

private:
    void initSocket();
    void readAsync();
    void asyncSendDone( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesWritten );
    void onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead );

    void processState();
    void processHeader();
    void processData();

private:
    quint16 m_requestTransactionId;
    quint16 m_requestFunctionCode;
    quint16 m_responseLength;

    ModbusClientState m_state;
    QnByteArray m_recvBuffer;
    std::shared_ptr<AbstractStreamSocket> m_socket;
};

}


