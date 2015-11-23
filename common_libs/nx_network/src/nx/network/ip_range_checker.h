#ifndef ip_range_checker_h_1427
#define ip_range_checker_h_1427

#include <QtCore/QObject>
#include <QtCore/QFuture>

#include <QtNetwork/QHostInfo>

class NX_NETWORK_API QnIpRangeChecker {
public:
    static QFuture<QStringList> onlineHostsAsync(const QHostAddress &startAddr, const QHostAddress &endAddr, int port);
};


#endif //ip_range_checker_h_1427
