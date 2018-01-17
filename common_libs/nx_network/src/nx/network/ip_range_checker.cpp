#include "ip_range_checker.h"

#include <QtNetwork/QTcpSocket>
#include <QtCore/QThreadPool>
#include <QtConcurrent/QtConcurrentMap>

#include "socket.h"
#include "nx/network/deprecated/simple_http_client.h"

struct QnTestAddress
{
    quint32 ip;
    int port;

    QnTestAddress(quint32 ip, int port): ip(ip), port(port) {}
};

QStringList mapFunction(QnTestAddress address)
{
    CLSimpleHTTPClient http(QHostAddress(address.ip), address.port, 1500, QAuthenticator());
    CLHttpStatus status = http.doGET(QByteArray(""));
    if (status == CL_TRANSPORT_ERROR)
        return QStringList();
    return QStringList() << QHostAddress(address.ip).toString();
}

void reduceFunction(QStringList& finalResult, const QStringList& intermediateResult)
{
    finalResult << intermediateResult;
}

QFuture<QStringList> QnIpRangeChecker::onlineHostsAsync(
    const QHostAddress& startAddr,
    const QHostAddress& endAddr,
    int port)
{
    QList<QnTestAddress> candidates;
    for (quint32 i = startAddr.toIPv4Address(); i <= endAddr.toIPv4Address(); ++i)
        candidates << QnTestAddress(i, port);
    return QtConcurrent::mappedReduced(candidates, &mapFunction, &reduceFunction);
}
