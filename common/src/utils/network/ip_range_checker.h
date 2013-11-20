#ifndef ip_range_checker_h_1427
#define ip_range_checker_h_1427

#include <QtNetwork/QHostInfo>
#include <QtCore/QObject>


class QnIpRangeChecker : public QObject
{
    Q_OBJECT
public:
    QnIpRangeChecker();
    ~QnIpRangeChecker();
    QStringList onlineHosts(const QHostAddress &startAddr, const QHostAddress &endAddr, int port);
private:

};


#endif //ip_range_checker_h_1427
