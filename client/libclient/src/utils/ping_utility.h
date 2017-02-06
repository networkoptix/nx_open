#ifndef PING_UTILITY_H
#define PING_UTILITY_H

#include <QThread>

class QnPingUtility : public QThread {
    Q_OBJECT
public:

    enum ResponceCode {
        UnknownError = -1,
        Timeout = -2
    };

    struct PingResponce {
        int type;
        quint16 code;
        quint32 bytes;
        QString hostAddress;
        quint16 seq;
        quint32 ttl;
        qreal time;
    };

    explicit QnPingUtility(QObject *parent = 0);

    void setHostAddress(const QString &hostAddress);

    void pleaseStop();

protected:
    virtual void run() override;

signals:
    void pingResponce(const PingResponce &responce);

private:
    PingResponce ping(quint16 seq);

private:
    bool m_stop;
    QString m_hostAddress;
    int m_timeout;
};

Q_DECLARE_METATYPE(QnPingUtility::PingResponce)

#endif // PING_UTILITY_H
