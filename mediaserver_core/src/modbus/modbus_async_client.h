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

    ~QnModbusAsyncClient();

    void setEndpoint(const SocketAddress& endpoint);

    void doModbusRequestAsync(const ModbusRequest& request);

    void readDiscreteInputsAsync();

    void readCoilsAsync(quint16 startCoil, quint16 coilNumber);
    void writeCoilsAsync();

    void readInputRegistersAsync();

    void readHoldingRegistersAsync(quint32 startRegister, quint32 registerCount);
    void writeHoldingRegistersAsync(quint32 startRegister, const QByteArray& data);

    QString getLastErrorString() const;

    void terminate();

signals:
    void done(ModbusResponse response);
    void error();

private:
    bool initSocket();
    void readAsync(quint64 currentRequestSequenceNum);
    void asyncSendDone(
        AbstractSocket* sock, 
        quint64 currentRequestSequenceNum, 
        SystemError::ErrorCode errorCode, 
        size_t bytesWritten);

    void onSomeBytesReadAsync(
        AbstractSocket* sock, 
        quint64 currentRequestSequenceNum,
        SystemError::ErrorCode errorCode, 
        size_t bytesRead);

    void processState(quint64 currentRequestSequenceNum);
    void processHeader(quint64 currentRequestSequenceNum);
    void processData(quint64 currentRequestSequenceNum);

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
    mutable QnMutex m_mutex;
    quint64 m_requestSequenceNum;
    std::shared_ptr<AbstractStreamSocket> m_socket;
    bool m_needReinitSocket;
    bool m_terminated;
};

}


