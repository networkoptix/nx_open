/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef ONVIF_NOTIFICATION_CONSUMER_H
#define ONVIF_NOTIFICATION_CONSUMER_H

#include <map>

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
    /*!
        Triggeres input business event
    */
	virtual	int Notify( _oasisWsnB2__Notify* oasisWsnB2__Notify );

    //!Pass notifications from address \a notificationProducerAddress to \a resource
    void registerResource(
        QnPlOnvifResource* const resource,
        const QString& notificationProducerAddress );
    //!Cancel registration of \a resource
    void removeResourceRegistration( QnPlOnvifResource* const resource );

private:
    std::map<QString, QnPlOnvifResource*> m_notificationProducerAddressToResource;
};

#endif  //ONVIF_NOTIFICATION_CONSUMER_H
