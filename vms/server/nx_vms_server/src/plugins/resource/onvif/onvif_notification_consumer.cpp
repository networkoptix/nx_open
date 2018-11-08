/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "onvif_notification_consumer.h"

#include <nx/utils/thread/mutex.h>

#include <nx/network/system_socket.h>

#include "onvif_resource.h"
#include <nx/utils/log/log.h>

using std::map;
using std::string;

NotificationConsumerBindingService* OnvifNotificationConsumer::copy()
{
    //TODO/IMPL
    NX_ASSERT( false );
    return NULL;
}

int OnvifNotificationConsumer::Notify( _oasisWsnB2__Notify* notificationRequest )
{
    NX_DEBUG(this, lit("Received soap notification from %1").
        arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")));

    QnMutexLocker lk( &m_mutex );

    //oasisWsnB2__Notify->
    for( size_t i = 0; i < notificationRequest->NotificationMessage.size(); ++i )
    {
        if( !notificationRequest->NotificationMessage[i] )
            continue;
        const oasisWsnB2__NotificationMessageHolderType& notification = *notificationRequest->NotificationMessage[i];
        if( (!notification.ProducerReference ||
             !notification.ProducerReference->Address) &&
            (!notification.SubscriptionReference ||
             !notification.SubscriptionReference->Address) &&
            !(notificationRequest->soap && notificationRequest->soap->host) )
        {
            NX_WARNING(this, lit("Received notification with no producer reference and no subscription reference (endpoint %1). Unable to associate with resource. Ignoring...").
                arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")));
            continue;
        }

        QWeakPointer<QnPlOnvifResource> resToNotify;

        //searching for resource by address
        if( notification.ProducerReference && notification.ProducerReference->Address )
        {
            const QString& address = QUrl( QString::fromStdString(notification.ProducerReference->Address) ).host();
            auto it = m_notificationProducerAddressToResource.find( address );
            if( it != m_notificationProducerAddressToResource.end() )
                resToNotify = it->second;
        }

        if( !resToNotify && notification.SubscriptionReference && notification.SubscriptionReference->Address )
        {
            //trying to find by subscription reference
            auto it = m_subscriptionReferenceToResource.find( QString::fromStdString(notification.SubscriptionReference->Address) );
            if( it != m_subscriptionReferenceToResource.end() )
                resToNotify = it->second;
        }

        if( !resToNotify && notificationRequest->soap && notificationRequest->soap->host )
        {
            //searching by host
            auto it = m_notificationProducerAddressToResource.find( QLatin1String(notificationRequest->soap->host) );
            if( it != m_notificationProducerAddressToResource.end() )
                resToNotify = it->second;
        }

        if( resToNotify )
        {
            lk.unlock();
            {
                auto resStrongRef = resToNotify.toStrongRef();
                if( resStrongRef )
                    resStrongRef->handleOneNotification( notification );
            }
            lk.relock();
        }
        else
        {
            //this is possible shortly after resource unregistration
            NX_WARNING (this, lit("Received notification for unknown resource. Ignoring..."));
        }
    }

    return SOAP_OK;
}

//!Pass notifications from address \a notificationProducerAddress to \a resource
void OnvifNotificationConsumer::registerResource(
    const QnPlOnvifResourcePtr& resource,
    const QString& notificationProducerAddress,
    const QString& subscriptionReference )
{
    QnMutexLocker lk( &m_mutex );
    auto resWeakRef = resource.toWeakRef();
    m_notificationProducerAddressToResource[notificationProducerAddress] = resWeakRef;
    if( !subscriptionReference.isEmpty() )
        m_subscriptionReferenceToResource[subscriptionReference] = resWeakRef;
}

//!Cancel registration of \a resource
void OnvifNotificationConsumer::removeResourceRegistration( const QnPlOnvifResourcePtr& resource )
{
    QnMutexLocker lk( &m_mutex );

    auto compareResourceFunc = [resource]( const std::pair<QString, QnPlOnvifResourcePtr>& val ){ return val.second == resource; };

    auto it = std::find_if(
        m_notificationProducerAddressToResource.begin(),
        m_notificationProducerAddressToResource.end(),
        compareResourceFunc );
    if( it != m_notificationProducerAddressToResource.end() )
        m_notificationProducerAddressToResource.erase( it );

    auto it1 = std::find_if(
        m_subscriptionReferenceToResource.begin(),
        m_subscriptionReferenceToResource.end(),
        compareResourceFunc );
    if( it1 != m_subscriptionReferenceToResource.end() )
        m_subscriptionReferenceToResource.erase( it1 );
}

SOAP_SOCKET OnvifNotificationConsumer::accept()
{
    return nx::network::TCPServerSocket::accept(soap->master);
}

#endif  //ENABLE_ONVIF
