#ifndef ip_range_checker_h_1427
#define ip_range_checker_h_1427

class QnIprangeChecker : public QObject
{
    Q_OBJECT
public:
    QnIprangeChecker();
    ~QnIprangeChecker();
    QList<QString> onlineHosts(QHostAddress startAddr, QHostAddress endAddr);
private:

};


#endif //ip_range_checker_h_1427