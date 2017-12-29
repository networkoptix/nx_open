#ifndef EIP_ASYNC_CLIENT_H
#define EIP_ASYNC_CLIENT_H

#include "eip_cip.h"

class EIPAsyncClient: public QObject
{
Q_OBJECT

enum class EIPClientState
{
    NeedSession,
    WaitingForSession,
    ReadyToRequest,

    ReadingHeader,
    ReadingData,
    DataWasRead,

    Error
};

public:
    EIPAsyncClient(QString hostAddress);
    ~EIPAsyncClient();
    void setPort(quint16 port);
    bool doServiceRequestAsync(const MessageRouterRequest& request);
    MessageRouterResponse getResponse();

signals:
    void done();

private:
    QString m_hostAddress;
    quint16 m_port;
    QnMutex m_mutex;

    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    bool m_terminated;
    bool m_inProcess;
    nx::Buffer m_sendBuffer;
    nx::Buffer m_recvBuffer;

    bool m_hasPendingRequest;
    MessageRouterResponse m_response;
    MessageRouterRequest m_pendingRequest;

    size_t m_dataLength;
    EIPClientState m_currentState;

    eip_session_handle_t m_sessionHandle;

private:
    bool initSocket();
    void terminate();
    nx::Buffer buildEIPServiceRequest(const MessageRouterRequest& request) const;
    nx::Buffer buildEIPRegisterSessionRequest();


    bool registerSessionAsync();
    bool doServiceRequestAsyncInternal(const MessageRouterRequest& request);
    void sendPendingRequest();

    void asyncConnectDone(SystemError::ErrorCode errorCode);

    void asyncSendDone(
        SystemError::ErrorCode errorCode,
        size_t bytesWritten);

    void onSomeBytesReadAsync(
        SystemError::ErrorCode errorCode,
        size_t bytesRead);

    void processState();
    void processPacket(const nx::Buffer& buf);

    void processHeaderBytes();
    void processDataBytes();
    void processSessionBytes();

    void resetBuffers();
};

#endif // EIP_ASYNC_CLIENT_H
