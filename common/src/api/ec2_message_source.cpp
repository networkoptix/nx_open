#include "ec2_message_source.h"

QnMessageSource2::QnMessageSource2(ec2::AbstractECConnectionPtr connection)
{
    connect(connection.get(), &ec2::AbstractECConnection::initNotification, this, &QnMessageSource2::at_gotNotification);
    connection->startReceivingNotifications(true);
}

void QnMessageSource2::at_gotNotification(ec2::QnFullResourceData fullData)
{
    QnMessage message;
    message.messageType = Qn::Message_Type_Initial;
    message.resourceTypes = fullData.resTypes;
    message.resources = fullData.resources;
    message.licenses = fullData.licenses;
    message.cameraServerItems = fullData.cameraHistory;
    message.businessRules = fullData.bRules;
    emit messageReceived(message);
}
