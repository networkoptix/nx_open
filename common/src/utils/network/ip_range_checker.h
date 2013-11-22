#ifndef ip_range_checker_h_1427
#define ip_range_checker_h_1427

#include <QtCore/QObject>
#include <QtCore/QFuture>

#include <QtNetwork/QHostInfo>

class QnIpRangeChecker {
public:
    static QStringList onlineHosts(const QHostAddress &startAddr, const QHostAddress &endAddr, int port);

    static QFuture<QStringList> onlineHostsAsync(const QHostAddress &startAddr, const QHostAddress &endAddr, int port);
private:

};


#endif //ip_range_checker_h_1427
