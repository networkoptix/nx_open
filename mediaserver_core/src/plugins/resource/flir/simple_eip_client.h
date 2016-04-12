#ifndef SIMPLE_EIP_CLIENT_H
#define SIMPLE_EIP_CLIENT_H

#include "eip_cip.h"

class SimpleEIPClient
{

public:
    static const quint16 DEFAULT_EIP_PORT = 0xAF12;
    static const qint8 DEFAULT_EIP_TIMEOUT = 4;
    static const size_t BUFFER_SIZE = 1024*16;

public:
    SimpleEIPClient(QHostAddress addr);
    ~SimpleEIPClient();
    bool connect();
    bool registerSession();
    bool unregisterSession();
    void setPort(const quint16 port);
    MessageRouterResponse doServiceRequest(
        quint8 serviceId,
        quint8 classId,
        quint8 instanceId,
        quint8 attributeId,
        const QByteArray& data);

    MessageRouterResponse doServiceRequest(const MessageRouterRequest& request);

private:
    QHostAddress m_hostAddress;
    quint16 m_port;
    QnMutex m_mutex;
    quint32 m_sessionHandle;
    TCPSocketPtr m_eipSocket;
    bool m_connected;
    char m_recvBuffer[BUFFER_SIZE];


private:
    void initSocket();
    MessageRouterRequest buildMessageRouterRequest(
        const quint8 serviceId,
        const quint8 classId,
        const quint8 instanceId,
        const quint8 attributeId,
        const QByteArray& data
    ) const;

    EIPPacket buildEIPEncapsulatedPacket(
        const MessageRouterRequest& request
    ) const;

    MessageRouterResponse getServiceResponseData(const QByteArray& response) const;

    eip_status_t tryGetResponse(const MessageRouterRequest& req, QByteArray& data);

};

#endif // SIMPLE_EIP_CLIENT_H
