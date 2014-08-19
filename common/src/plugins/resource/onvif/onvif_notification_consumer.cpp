/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "onvif_notification_consumer.h"

#include <QtCore/QMutexLocker>

#include <utils/network/system_socket.h>

#include "onvif_resource.h"
#include "../../../utils/common/log.h"


using std::map;
using std::string;

NotificationConsumerBindingService* OnvifNotificationConsumer::copy()
{
    //TODO/IMPL
    Q_ASSERT( false );
    return NULL;
}

int OnvifNotificationConsumer::Notify( _oasisWsnB2__Notify* notificationRequest )
{
    NX_LOG( lit("Received soap notification from %1").
        arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")), cl_logDEBUG1 );

    QMutexLocker lk( &m_mutex );

    //oasisWsnB2__Notify->
    for( size_t i = 0; i < notificationRequest->oasisWsnB2__NotificationMessage.size(); ++i )
    {
        if( !notificationRequest->oasisWsnB2__NotificationMessage[i] )
            continue;
        const oasisWsnB2__NotificationMessageHolderType& notification = *notificationRequest->oasisWsnB2__NotificationMessage[i];
        if( (!notification.oasisWsnB2__ProducerReference ||
             !notification.oasisWsnB2__ProducerReference->Address) &&
            (!notification.oasisWsnB2__SubscriptionReference ||
             !notification.oasisWsnB2__SubscriptionReference->Address) &&
            !(notificationRequest->soap && notificationRequest->soap->host) )
        {
            NX_LOG( lit("Received notification with no producer reference and no subscription reference (endpoint %1). Unable to associate with resource. Ignoring...").
                arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")), cl_logWARNING );
            continue;
        }

        QWeakPointer<QnPlOnvifResource> resToNotify;

        //searching for resource by address
        if( notification.oasisWsnB2__ProducerReference && notification.oasisWsnB2__ProducerReference->Address )
        {
            const QString& address = QUrl( QString::fromStdString(notification.oasisWsnB2__ProducerReference->Address->__item) ).host();
            auto it = m_notificationProducerAddressToResource.find( address );
            if( it != m_notificationProducerAddressToResource.end() )
            {
                resToNotify = it->second;
                continue;
            }
        }

        if( !resToNotify && notification.oasisWsnB2__SubscriptionReference && notification.oasisWsnB2__SubscriptionReference->Address )
        {
            //trying to find by subscription reference
            auto it = m_subscriptionReferenceToResource.find( QString::fromStdString(notification.oasisWsnB2__SubscriptionReference->Address->__item) );
            if( it != m_subscriptionReferenceToResource.end() )
            {
                resToNotify = it->second;
                continue;
            }
        }

        if( !resToNotify && notificationRequest->soap && notificationRequest->soap->host )
        {
            //searching by host
            auto it = m_notificationProducerAddressToResource.find( QLatin1String(notificationRequest->soap->host) );
            if( it != m_notificationProducerAddressToResource.end() )
            {
                resToNotify = it->second;
                continue;
            }
        }

        if( resToNotify )
        {
            lk.unlock();
            {
                auto resStrongRef = resToNotify.toStrongRef();
                if( resStrongRef )
                    resStrongRef->notificationReceived( notification );
            }
            lk.relock();
        }

        //this is possible shortly after resource unregistration
        NX_LOG( lit("Received notification for unknown resource. Ignoring..."), cl_logWARNING );
    }

    return SOAP_OK;
}

//!Pass notifications from address \a notificationProducerAddress to \a resource
void OnvifNotificationConsumer::registerResource(
    const QnPlOnvifResourcePtr& resource,
    const QString& notificationProducerAddress,
    const QString& subscriptionReference )
{
    QMutexLocker lk( &m_mutex );
    auto resWeakRef = resource.toWeakRef();
    m_notificationProducerAddressToResource[notificationProducerAddress] = resWeakRef;
    if( !subscriptionReference.isEmpty() )
        m_subscriptionReferenceToResource[subscriptionReference] = resWeakRef;
}

//!Cancel registration of \a resource
void OnvifNotificationConsumer::removeResourceRegistration( const QnPlOnvifResourcePtr& resource )
{
    QMutexLocker lk( &m_mutex );

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
    return TCPServerSocket::accept(soap->master);
}

#endif  //ENABLE_ONVIF
