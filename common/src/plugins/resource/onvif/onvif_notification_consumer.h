/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef ONVIF_NOTIFICATION_CONSUMER_H
#define ONVIF_NOTIFICATION_CONSUMER_H

#ifdef ENABLE_ONVIF

#include <map>

#include <QtCore/QMutex>

#include <core/resource/resource_fwd.h>
#include <soapNotificationConsumerBindingService.h>


//!Processes notifications from onvif resources
class OnvifNotificationConsumer
:
    public NotificationConsumerBindingService
{
public:
    //!Implementation of NotificationConsumerBindingService::copy
    virtual NotificationConsumerBindingService* copy();
    //!Implementation of NotificationConsumerBindingService::Notify
    virtual int Notify( _oasisWsnB2__Notify* oasisWsnB2__Notify ) override;

    //!Pass notifications from address \a notificationProducerAddress to \a resource
    void registerResource(
        const QnPlOnvifResourcePtr& resource,
        const QString& notificationProducerAddress,
        const QString& subscriptionReference = QString() );
    //!Cancel registration of \a resource
    void removeResourceRegistration( const QnPlOnvifResourcePtr& resource );
    virtual SOAP_SOCKET accept() override;

private:
    std::map<QString, QWeakPointer<QnPlOnvifResource>> m_notificationProducerAddressToResource;
    std::map<QString, QWeakPointer<QnPlOnvifResource>> m_subscriptionReferenceToResource;
    mutable QMutex m_mutex;
};

#endif  //ENABLE_ONVIF

#endif  //ONVIF_NOTIFICATION_CONSUMER_H
