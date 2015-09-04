#ifndef __MULTICAST_HTTP_TRANSPORT_H_
#define __MULTICAST_HTTP_TRANSPORT_H_

#include <QElapsedTimer>
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

        QUuid addRequest(const QString& method, const Request& request, ResponseCallback callback, int timeoutMs);
        void addResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
    private slots:
        void sendNextData();
        void at_socketReadyRead();
        void at_dataSent(qint64 bytes);
        void at_timer();
    private:
        struct TransportConnection
        {
            TransportConnection(): receivedDataSize(0), timeoutMs(0) { timer.restart(); }
            bool hasExpired() const { return timeoutMs > 0 && timer.hasExpired(timeoutMs); }

            QUuid requestId;
            QQueue<QByteArray> dataToSend;
            QByteArray receivedData;
            int receivedDataSize;
            ResponseCallback responseCallback;
            int timeoutMs;
            QElapsedTimer timer;
        };
        
        QMap<QUuid, TransportConnection> m_requests;
        QUuid m_localGuid;
        qint64 m_bytesWritten;

        QUdpSocket m_socket;
        RequestCallback m_requestCallback;
        QTimer m_timer;
    private:
        QByteArray encodeMessage(const QString& method, const Request& request) const;
        Response decodeResponse(const TransportConnection& transportData, bool* ok) const;
        Request decodeRequest(const TransportConnection& transportData, bool* ok) const;
        TransportConnection encodeRequest(const QString& method, const Request& request);
        TransportConnection encodeResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse);
    };

}

#endif // __MULTICAST_HTTP_TRANSPORT_H_
