#ifndef __MULTICAST_HTTP_TRANSPORT_H_
#define __MULTICAST_HTTP_TRANSPORT_H_

#include <QElapsedTimer>
#include <QUdpSocket>
#include <QTimer>
#include "multicast_http_fwd.h"
#include <memory>
#include <QCache>

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

        static const int MAX_DATAGRAM_SIZE = 1412;
        static const int MAX_PAYLOAD_SIZE = MAX_DATAGRAM_SIZE - 167;

        QUuid magic;
        int version;
        QUuid requestId;
        QUuid clientId;
        QUuid serverId;
        MessageType messageType;
        int messageSize;
        int offset;
        QByteArray payloadData;

        QByteArray serialize() const;
        static Packet deserialize(const QByteArray& deserialize, bool* ok);
    };


    class Transport: public QObject
    {
        Q_OBJECT
    public:

        Transport(const QUuid& localGuid);

        QUuid addRequest(const Request& request, ResponseCallback callback, int timeoutMs);
        void addResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
        void setRequestCallback(RequestCallback value) { m_requestCallback = value; }
    private slots:
        void sendNextData();
        void at_socketReadyRead();
        void at_dataSent(qint64 bytes);
        void at_timer();
    private:

        struct TransportPacket
        {
            TransportPacket(): socket(0) {}
            TransportPacket(QUdpSocket* socket, const QByteArray& data): socket(socket), data(data) {}
            QUdpSocket* socket;
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
        
        QMap<QUuid, TransportConnection> m_requests;
        QUuid m_localGuid;
        qint64 m_bytesWritten;

        std::unique_ptr<QUdpSocket> m_recvSocket;
        std::vector<std::unique_ptr<QUdpSocket>> m_sendSockets;
        RequestCallback m_requestCallback;
        std::unique_ptr<QTimer> m_timer;
        QCache<QUuid, void> m_processedRequests;
        mutable QMutex m_mutex;
    private:
        QByteArray encodeMessage(const Request& request) const;
        Response decodeResponse(const TransportConnection& transportData, bool* ok) const;
        Request decodeRequest(const TransportConnection& transportData, bool* ok) const;
        TransportConnection encodeRequest(const Request& request);
        TransportConnection encodeResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
        void putPacketToTransport(TransportConnection& transportConnection, const Packet& packet);
    };

}

#endif // __MULTICAST_HTTP_TRANSPORT_H_
