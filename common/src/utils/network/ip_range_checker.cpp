
#include "ip_range_checker.h"

#include <QtNetwork/QTcpSocket>
#include <QtCore/QThreadPool>
#include <QtConcurrent/QtConcurrentMap>

#include "../common/sleep.h"
#include "socket.h"
#include "simple_http_client.h"

#include <utils/common/scoped_thread_rollback.h>


struct QnIpRangeCheckerHelper
{
    QnIpRangeCheckerHelper(quint32 _ip, int _port): ip(_ip), port(_port), online(false) {}

    quint32 ip;
    int port;
    bool online;
    QHostAddress discoveryAddress;
    QStringList results;

    QStringList check()
    {
        results.clear();
        online = false;
        CLSimpleHTTPClient http (QHostAddress(ip), port, 1500, QAuthenticator());
        CLHttpStatus status = http.doGET(QByteArray(""));
        if (status != CL_TRANSPORT_ERROR) 
        {
            //event if we received not http, considering camera alive
            discoveryAddress = http.getLocalHost();
            online = true;
        }
        if (online)
            results << QHostAddress(ip).toString();
        return results;
    }
};

struct QnTestAddress {
    quint32 ip;
    int port;

    QnTestAddress(quint32 ip, int port): ip(ip), port(port) {}
};

QStringList mapFunction(QnTestAddress address) {
    CLSimpleHTTPClient http (QHostAddress(address.ip), address.port, 1500, QAuthenticator());
    CLHttpStatus status = http.doGET(QByteArray(""));
    if (status == CL_TRANSPORT_ERROR)
        return QStringList();

    return QStringList() << QHostAddress(address.ip).toString();
}

void reduceFunction(QStringList &finalResult, const QStringList &intermediateResult) {
    finalResult << intermediateResult;
}

QStringList QnIpRangeChecker::onlineHosts(const QHostAddress &startAddr, const QHostAddress &endAddr, int port)
{
    // sorry; did not have time to mess with asiynch qt sockets + thread isuses;
    // so bad(slow) implementaiton here 
    quint32 startIpv4 = startAddr.toIPv4Address();
    quint32 endIpv4 = endAddr.toIPv4Address();

    if (endIpv4 < startIpv4)
        return QStringList();

    QList<QnIpRangeCheckerHelper> candidates;

    for (quint32 i = startIpv4; i <= endIpv4; ++i)
    {
        QnIpRangeCheckerHelper helper(i, port);
        candidates.push_back(helper);
    }


    static const int SCAN_THREAD_AMOUNT = 32;
    QN_INCREASE_MAX_THREADS(SCAN_THREAD_AMOUNT)

    QtConcurrent::blockingMap(candidates, &QnIpRangeCheckerHelper::check);

    QStringList result;
    foreach(QnIpRangeCheckerHelper h, candidates)
        if(h.online)
            result << QHostAddress(h.ip).toString();

    return result;
}

QFuture<QStringList> QnIpRangeChecker::onlineHostsAsync(const QHostAddress &startAddr, const QHostAddress &endAddr, int port) {
    quint32 startIpv4 = startAddr.toIPv4Address();
    quint32 endIpv4 = endAddr.toIPv4Address();
    QList<QnTestAddress> candidates;
    for (quint32 i = startIpv4; i <= endIpv4; ++i)
    {
        QnTestAddress helper(i, port);
        candidates.push_back(helper);
    }

    return QtConcurrent::mappedReduced(candidates, &mapFunction, &reduceFunction);
}
