
#include "ip_range_checker.h"

#include <QTcpSocket>
#include <QThreadPool>
#include <QtConcurrentMap>

#include "../common/sleep.h"
#include "socket.h"
#include "simple_http_client.h"


struct QnIprangeCheckerHelper 
{
    QnIprangeCheckerHelper(quint32 _ip, int _port): ip(_ip), port(_port), online(false) {}

    quint32 ip;
    int port;
    bool online;
    QHostAddress discoveryAddress;

    void check()
    {
        online = false;
        
        QString ips = QHostAddress(ip).toString();

        CLSimpleHTTPClient http (QHostAddress(ip), port, 1500, QAuthenticator());
        CLHttpStatus status = http.doGET(QByteArray(""));
        if (status != CL_TRANSPORT_ERROR) 
        {
            discoveryAddress = http.getLocalHost();
            QString gg = discoveryAddress.toString();
            online = true;
        }

    }
};


QnIprangeChecker::QnIprangeChecker()
{

}

QnIprangeChecker::~QnIprangeChecker()
{

}

QList<QString> QnIprangeChecker::onlineHosts(QHostAddress startAddr, QHostAddress endAddr, int port)
{
    // sorry; did not have time to mess with asiynch qt sockets + thread isuses;
    // so bad(slow) implementaiton here 
    quint32 startIpv4 = startAddr.toIPv4Address();
    quint32 endIpv4 = endAddr.toIPv4Address();

    if (endIpv4 < startIpv4)
        return QList<QString>();

    QList<QnIprangeCheckerHelper> candidates;

    for (quint32 i = startIpv4; i <= endIpv4; ++i)
    {
        QnIprangeCheckerHelper helper(i, port);
        candidates.push_back(helper);
    }


    int threads = 32;
    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(candidates, &QnIprangeCheckerHelper::check);
    for (int i = 0; i < threads; ++i )global->reserveThread();


    QList<QString> result;


    foreach(QnIprangeCheckerHelper h, candidates)
    {
        if(h.online)
            result.push_back(QHostAddress(h.ip).toString());
    }

    return result;
}

