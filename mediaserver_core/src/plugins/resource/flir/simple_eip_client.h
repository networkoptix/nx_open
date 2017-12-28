#ifndef SIMPLE_EIP_CLIENT_H
#define SIMPLE_EIP_CLIENT_H

#include "eip_cip.h"
#include <nx/network/abstract_socket.h>

class SimpleEIPClient
{

public:
    static const quint16 kDefaultEipPort = 0xAF12;
    static const qint8 kDefaultEipTimeout = 4;
    static const size_t kBufferSize = 1024*16;

public:
    SimpleEIPClient(QString addr);
    ~SimpleEIPClient();
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
    QString m_hostAddress;
    quint16 m_port;
    QnMutex m_mutex;
    quint32 m_sessionHandle;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_eipSocket;
    bool m_connected;
    char m_recvBuffer[kBufferSize];


private:
    bool initSocket();
    bool connectIfNeeded();

    bool sendAll(nx::network::AbstractStreamSocket* socket, QByteArray& data);
    bool receiveMessage(nx::network::AbstractStreamSocket* socket, char* const buffer);
    void handleSocketError();

    bool registerSessionUnsafe();

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

    bool tryGetResponse(const MessageRouterRequest& req, QByteArray& data, eip_status_t* outEipStatus=nullptr);

};

#endif // SIMPLE_EIP_CLIENT_H
