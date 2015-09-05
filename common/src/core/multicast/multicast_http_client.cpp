#include "multicast_http_client.h"

namespace QnMulticast
{

HTTPClient::HTTPClient(const QUuid& localGuid):
    m_transport(localGuid)
{

}

QUuid HTTPClient::execRequest(const Request& request, ResponseCallback callback, int timeoutMs)
{
    return m_transport.addRequest(request, callback, timeoutMs);
}

}
