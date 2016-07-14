#ifndef EIP_ASYNC_CLIENT_H
#define EIP_ASYNC_CLIENT_H

#include "eip_cip.h"

class EIPAsyncClient: public QObject
{
Q_OBJECT

enum class EIPClientState {
    WaitingForSession,
    ReadingSession,
    ReadyToRequest,

    WaitingForResponse,
    NeedSession,

    WaitingForHeader,
    ReadingHeader,
    WaitingForData,
    ReadingData,
    DataWasRead,
    Error
};

public:
    EIPAsyncClient(QHostAddress hostAddress);
    ~EIPAsyncClient();
    static const unsigned int kEipPort = 44818;
    void setPort(quint16 port);
    bool doServiceRequestAsync(const MessageRouterRequest& request);
    MessageRouterResponse getResponse();

signals:
    void done();

private:
    QHostAddress m_hostAddress;
    quint16 m_port;
    QnMutex m_mutex;

    std::shared_ptr<AbstractStreamSocket> m_socket;
    bool m_terminated;
    bool m_inProcess;
    nx::Buffer m_sendBuffer;
    nx::Buffer m_recvBuffer;

    nx::Buffer m_headerBuffer;
    nx::Buffer m_dataBuffer;

    bool m_responseFound;
    bool m_hasPendingRequest;
    MessageRouterResponse m_response;
    MessageRouterRequest m_pendingRequest;

    size_t m_dataLength;
    EIPClientState m_currentState;

    eip_status_t m_eipStatus;
    eip_session_handle_t m_sessionHandle;

private:
    void initSocket();
    void terminate();
    nx::Buffer buildEIPServiceRequest(const MessageRouterRequest& request) const;
    nx::Buffer buildEIPRegisterSessionRequest();


    bool registerSessionAsync();
    bool doServiceRequestAsyncInternal(const MessageRouterRequest& request);
    void unregisterSession();
    void sendPendingRequest();

    void asyncSendDone(
        std::shared_ptr<AbstractStreamSocket> socket,
        SystemError::ErrorCode errorCode,
        size_t bytesWritten);

    void onSomeBytesReadAsync(
        std::shared_ptr<AbstractStreamSocket> socket,
        SystemError::ErrorCode errorCode,
        size_t bytesRead);

    void processState();
    void processPacket(const nx::Buffer& buf);

    void processHeaderBytes();
    void processDataBytes();
    void processSessionBytes();
};

#endif // EIP_ASYNC_CLIENT_H
