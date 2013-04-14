/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef ONVIF_NOTIFICATION_CONSUMER_H
#define ONVIF_NOTIFICATION_CONSUMER_H

#include <map>

#include <QMutex>

#include <soapNotificationConsumerBindingService.h>


class QnPlOnvifResource;

//!Processes notifications from onvif respurces
class OnvifNotificationConsumer
:
    public NotificationConsumerBindingService
{
public:
    //!Implementation of NotificationConsumerBindingService::copy
	virtual	NotificationConsumerBindingService* copy();
    //!Implementation of NotificationConsumerBindingService::Notify
	virtual	int Notify( _oasisWsnB2__Notify* oasisWsnB2__Notify );

    //!Pass notifications from address \a notificationProducerAddress to \a resource
    void registerResource(
        QnPlOnvifResource* const resource,
        const QString& notificationProducerAddress );
    //!Cancel registration of \a resource
    void removeResourceRegistration( QnPlOnvifResource* const resource );
    virtual	SOAP_SOCKET accept() override;

private:
    std::map<QString, QnPlOnvifResource*> m_notificationProducerAddressToResource;
    mutable QMutex m_mutex;
};

#endif  //ONVIF_NOTIFICATION_CONSUMER_H
