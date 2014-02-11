#ifndef _EC2_MESSAGE_SOURCE_H__
#define _EC2_MESSAGE_SOURCE_H__

#include "nx_ec/ec_api.h"
#include "app_server_connection.h"
#include "message.h"

class QnMessageSource2 : public QObject
{
    Q_OBJECT
public:
    QnMessageSource2(ec2::AbstractECConnectionPtr connection);
signals:
    void connectionOpened(QnMessage message);
    void messageReceived(QnMessage message);
private slots:
    void at_gotNotification(ec2::QnFullResourceData fullData);
};


#endif // _EC2_MESSAGE_SOURCE_H__
