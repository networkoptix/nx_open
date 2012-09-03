#include "ip_range_checker.h"
#include <QTcpSocket>
#include "../common/sleep.h"
#include "socket.h"

struct QnIprangeCheckerHelper 
{

    quint32 ip;
    bool online;

    void check()
    {
        online = false;
        

        TCPSocket sock;
        sock.setReadTimeOut(1000);
        sock.setWriteTimeOut(1000);

        if (sock.connect(QHostAddress(ip).toString(), 80))
        {
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

QList<QHostAddress> QnIprangeChecker::onlineHosts(QHostAddress startAddr, QHostAddress endAddr)
{
    // sorry; did not have time to mess with asiynch qt sockets + thread isuses;
    // so bad(slow) implementaiton here 
    quint32 startIpv4 = startAddr.toIPv4Address();
    quint32 endIpv4 = endAddr.toIPv4Address();

    if (endIpv4 < startIpv4)
        return QList<QHostAddress>();

    QList<QnIprangeCheckerHelper> candidates;

    for (quint32 i = startIpv4; i <= endIpv4; ++i)
    {
        QnIprangeCheckerHelper helper;
        helper.online = false;
        helper.ip = i;
        candidates.push_back(helper);
    }


    int threads = 10;
    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(candidates, &QnIprangeCheckerHelper::check);
    for (int i = 0; i < threads; ++i )global->reserveThread();


    QList<QHostAddress> result;


    foreach(QnIprangeCheckerHelper h, candidates)
    {
        if(h.online)
            result.push_back(QHostAddress(h.ip));
    }

    foreach(QHostAddress addr, result)
    {
        qDebug() << addr.toString();
    }
    

    return result;
}

