#ifndef __MULTICAST_HTTP_TRANSPORT_H_
#define __MULTICAST_HTTP_TRANSPORT_H_

#include <QUdpSocket>
#include <QUuid>
#include "multicast_http_fwd.h"

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
        int maxPayloadSize() const;
    };


    class Transport: public QObject
    {
        Q_OBJECT
    public:

        Transport(const QUuid& localGuid);

        QUuid addRequest(const QString& method, const Request& request);

    private slots:
        void sendNextData();
        void at_socketReadyRead();
    private:
        struct TransportRequest
        {
            TransportRequest(): receivedDataSize(0) {}

            QUuid requestId;
            QUuid serverId;
            QQueue<QByteArray> dataToSend;
            QByteArray receivedData;
            int receivedDataSize;
            ResponseCallback callback;
        };
        
        QMap<QUuid, TransportRequest> m_requests;

        //QMap<QUuid, ResponseCallback> m_awaitingRequests;
        //QQueue<SendingRequest> m_sendingData;
        //QMap<QUuid, QByteArray> m_receivedData;

        QUuid m_localGuid;
        qint64 m_bytesWritten;

        QUdpSocket m_socket;
    private:
        QByteArray encodeMessage(const QString& method, const Request& request) const;
        QnMulticast::Response decodeMessage(const TransportRequest& transportData, bool* ok) const;
        TransportRequest encodeRequest(const QString& method, const Request& request);

    };

}

#endif // __MULTICAST_HTTP_TRANSPORT_H_
