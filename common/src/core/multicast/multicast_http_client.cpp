#include "multicast_http_client.h"

namespace QnMulticast
{

HTTPClient::HTTPClient(const QUuid& localGuid):
    m_transport(localGuid)
{

}

QUuid HTTPClient::doGet(const Request& request, ResponseCallback callback, int timeoutMs)
{
    return execRequest(QLatin1String("GET"), request, callback, timeoutMs);
}

QUuid HTTPClient::doPost(const Request& request, ResponseCallback callback, int timeoutMs)
{
    return execRequest(QLatin1String("POST"), request, callback, timeoutMs);
}

QUuid HTTPClient::execRequest(const QString& method, const Request& request, ResponseCallback callback, int timeoutMs)
{
    QUuid requestId = m_transport.addRequest(method, request);
    return requestId;
}

}
