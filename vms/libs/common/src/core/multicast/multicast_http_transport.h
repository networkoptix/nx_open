#pragma once

#include <QtCore/QElapsedTimer>
#include <QtNetwork/QUdpSocket>
#include <QTimer>
#include "multicast_http_fwd.h"
#include <memory>
#include <QCache>
#include <QLinkedList>

#include <nx/utils/thread/mutex.h>


namespace QnMulticast
{
    enum class MessageType
    {
        request,
        response
    };


    struct Packet
    {
        Packet();

        static const int MAX_DATAGRAM_SIZE;

        QUuid magic;
        int version;
        QUuid requestId;
        QUuid clientId;
        QUuid serverId;
        MessageType messageType;
        int messageSize;
        int offset;
        QByteArray payloadData;

        int maxPayloadSize() const;
        QByteArray serialize() const;
        static Packet deserialize(const QByteArray& deserialize, bool* ok);
    private:
        QByteArray serializeHeader() const;
    };


    class Transport: public QObject
    {
        Q_OBJECT
    public:

        Transport(const QUuid& localGuid);

        QUuid addRequest(const Request& request, ResponseCallback callback, int timeoutMs);
        void addResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
        void setRequestCallback(RequestCallback value) { m_requestCallback = value; }
        void cancelRequest(const QUuid& requestId);
        QString localAddress() const;
    private slots:
        void sendNextData();
        void at_socketReadyRead();
        void at_timer();
        void initSockets(const QSet<QString>& addrList);
    private:

        struct TransportPacket
        {
            TransportPacket(): socket(0) {}
            TransportPacket(std::shared_ptr<QUdpSocket> socket, const QByteArray& data): socket(socket), data(data) {}
            std::shared_ptr<QUdpSocket> socket;
            QByteArray data;
        };

        struct TransportConnection
        {
            TransportConnection(): timeoutMs(0) { timer.restart(); }
            bool hasExpired() const { return timeoutMs > 0 && timer.hasExpired(timeoutMs); }

            QUuid requestId;
            QQueue<TransportPacket> dataToSend;
            QByteArray receivedData;
            ResponseCallback responseCallback;
            int timeoutMs;
            QElapsedTimer timer;
        };

        QLinkedList<TransportConnection> m_requests;
        QUuid m_localGuid;

        std::unique_ptr<QUdpSocket> m_recvSocket;
        std::vector<std::shared_ptr<QUdpSocket>> m_sendSockets;
        RequestCallback m_requestCallback;
        std::unique_ptr<QTimer> m_timer;
        QCache<QUuid, char> m_processedRequests;
        mutable QnMutex m_mutex;
        bool m_nextSendQueued;
        QElapsedTimer m_checkInterfacesTimer;
        QSet<QString> m_localAddressList;
    private:
        QByteArray serializeMessage(const Request& request) const;
        TransportConnection serializeRequest(const Request& request);
        TransportConnection serializeResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
        bool parseHttpMessage(Message& result, const QByteArray& message) const;
        Response parseResponse(const TransportConnection& transportData, bool* ok) const;
        Request parseRequest(const TransportConnection& transportData, bool* ok) const;
        void putPacketToTransport(TransportConnection& transportConnection, const Packet& packet);
        void eraseRequest(const QUuid& id);
        void queueNextSendData(int delay);
    };

}
