
#include "ip_range_checker.h"

#include <QtNetwork/QTcpSocket>
#include <QtCore/QThreadPool>
#include <QtConcurrent/QtConcurrentMap>

#include "../common/sleep.h"
#include "socket.h"
#include "simple_http_client.h"


struct QnIpRangeCheckerHelper
{
    QnIpRangeCheckerHelper(quint32 _ip, int _port): ip(_ip), port(_port), online(false) {}

    quint32 ip;
    int port;
    bool online;
    QHostAddress discoveryAddress;

    void check()
    {
        online = false;
        CLSimpleHTTPClient http (QHostAddress(ip), port, 1500, QAuthenticator());
        CLHttpStatus status = http.doGET(QByteArray(""));
        if (status != CL_TRANSPORT_ERROR) 
        {
            //event if we received not http, considering camera alive
            discoveryAddress = http.getLocalHost();
            online = true;
        }
    }
};


QnIpRangeChecker::QnIpRangeChecker()
{

}

QnIpRangeChecker::~QnIpRangeChecker()
{

}

QStringList QnIpRangeChecker::onlineHosts(const QHostAddress &startAddr, const QHostAddress &endAddr, int port)
{
    // sorry; did not have time to mess with asiynch qt sockets + thread isuses;
    // so bad(slow) implementaiton here 
    quint32 startIpv4 = startAddr.toIPv4Address();
    quint32 endIpv4 = endAddr.toIPv4Address();

    if (endIpv4 < startIpv4)
        return QList<QString>();

    QList<QnIpRangeCheckerHelper> candidates;

    for (quint32 i = startIpv4; i <= endIpv4; ++i)
    {
        QnIpRangeCheckerHelper helper(i, port);
        candidates.push_back(helper);
    }


    static const int SCAN_THREAD_AMOUNT = 32;
    QThreadPool* global = QThreadPool::globalInstance();

    //  Calling this function without previously reserving a thread temporarily increases maxThreadCount().
    for (int i = 0; i < SCAN_THREAD_AMOUNT; ++i )
        global->releaseThread();

    QtConcurrent::blockingMap(candidates, &QnIpRangeCheckerHelper::check);

    // Returning maxThreadCount() to its original value
    for (int i = 0; i < SCAN_THREAD_AMOUNT; ++i )
        global->reserveThread();

    QStringList result;

    foreach(QnIpRangeCheckerHelper h, candidates)
        if(h.online)
            result << QHostAddress(h.ip).toString();

    return result;
}

