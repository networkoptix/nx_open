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

    //oasisWsnB2__Notify->
    for( size_t i = 0; i < notificationRequest->oasisWsnB2__NotificationMessage.size(); ++i )
    {
        if( !notificationRequest->oasisWsnB2__NotificationMessage[i] )
            continue;
        const oasisWsnB2__NotificationMessageHolderType& notification = *notificationRequest->oasisWsnB2__NotificationMessage[i];
        if( (!notification.oasisWsnB2__ProducerReference ||
             !notification.oasisWsnB2__ProducerReference->Address) &&
            (!notification.oasisWsnB2__SubscriptionReference ||
             !notification.oasisWsnB2__SubscriptionReference->Address) )
        {
            NX_LOG( lit("Received notification with no producer reference and no subscription reference (endpoint %1). Unable to associate with resource. Ignoring...").
                arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")), cl_logWARNING );
            continue;
        }

        //if( !notification.oasisWsnB2__Topic ||
        //    notification.oasisWsnB2__Topic->Dialect != "tns1:Device/Trigger/Relay" )
        //{
        //    NX_LOG( lit("Received notification with unknown topic: %1. Ignoring...").
        //        arg(QString::fromStdString(notification.oasisWsnB2__Topic ? notification.oasisWsnB2__Topic->Dialect : string())), cl_logDEBUG1 );
        //    continue;
        //}

        //TODO/IMPL searching for resource by address
        auto it = m_notificationProducerAddressToResource.end();
        if( notification.oasisWsnB2__ProducerReference && notification.oasisWsnB2__ProducerReference->Address )
        {
            const QString& address = QUrl( QString::fromStdString(notification.oasisWsnB2__ProducerReference->Address->__item) ).host();
            it = m_notificationProducerAddressToResource.find( address );
        }

        if( it == m_notificationProducerAddressToResource.end() )
        {
            //trying to find by subscription reference
            if( !(notification.oasisWsnB2__SubscriptionReference &&
                  notification.oasisWsnB2__SubscriptionReference->Address &&
                  (it = m_subscriptionReferenceToResource.find( QString::fromStdString(notification.oasisWsnB2__SubscriptionReference->Address->__item) )) != m_subscriptionReferenceToResource.end()) )
            {
                //this is possible shortly after resource unregistration
                //NX_LOG( lit("Received notification for unknown resource. Producer address %1. Ignoring...").
                //    arg(QString::fromStdString(notification.oasisWsnB2__ProducerReference->Address->__item)), cl_logWARNING );
                NX_LOG( lit("Received notification for unknown resource. Ignoring..."), cl_logWARNING );
                continue;
            }
        }

        //TODO/IMPL get relay token and state from notification
        it->second->notificationReceived( notification );
    }

    return SOAP_OK;
}

//!Pass notifications from address \a notificationProducerAddress to \a resource
void OnvifNotificationConsumer::registerResource(
    QnPlOnvifResource* const resource,
    const QString& notificationProducerAddress,
    const QString& subscriptionReference )
{
    QMutexLocker lk( &m_mutex );
    m_notificationProducerAddressToResource[notificationProducerAddress] = resource;
    if( !subscriptionReference.isEmpty() )
        m_subscriptionReferenceToResource[subscriptionReference] = resource;
}

//!Cancel registration of \a resource
void OnvifNotificationConsumer::removeResourceRegistration( QnPlOnvifResource* const resource )
{
    QMutexLocker lk( &m_mutex );

    auto compareResourceFunc = [resource]( const std::pair<QString, QnPlOnvifResource*>& val ){ return val.second == resource; };

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

