/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#include "onvif_notification_consumer.h"

#include <QMutexLocker>

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
    cl_log.log( QString::fromLatin1("Received soap notification from %1").
        arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")), cl_logDEBUG1 );

    //oasisWsnB2__Notify->
    for( size_t i = 0; i < notificationRequest->oasisWsnB2__NotificationMessage.size(); ++i )
    {
        if( !notificationRequest->oasisWsnB2__NotificationMessage[i] )
            continue;
        const oasisWsnB2__NotificationMessageHolderType& notification = *notificationRequest->oasisWsnB2__NotificationMessage[i];

        if( !notification.oasisWsnB2__ProducerReference ||
            !notification.oasisWsnB2__ProducerReference->Address )
        {
            cl_log.log( QString::fromLatin1("Received notification with no producer reference (endpoint %1). Unable to associate with resource. Ignoring...").
                arg(QString::fromLatin1(notificationRequest->soap ? notificationRequest->soap->endpoint : "")), cl_logWARNING );
            continue;
        }

        //if( !notification.oasisWsnB2__Topic ||
        //    notification.oasisWsnB2__Topic->Dialect != "tns1:Device/Trigger/Relay" )
        //{
        //    cl_log.log( QString::fromLatin1("Received notification with unknown topic: %1. Ignoring...").
        //        arg(QString::fromStdString(notification.oasisWsnB2__Topic ? notification.oasisWsnB2__Topic->Dialect : string())), cl_logDEBUG1 );
        //    continue;
        //}

        //TODO/IMPL searching for resource by address
        const QString& address = QUrl( QString::fromStdString(notification.oasisWsnB2__ProducerReference->Address->__item) ).host();
        map<QString, QnPlOnvifResource*>::const_iterator it = m_notificationProducerAddressToResource.find( address );
        if( it == m_notificationProducerAddressToResource.end() )
        {
            //this is possible shortly after resource unregistration
            cl_log.log( QString::fromLatin1("Received notification for unknown resource. Producer address %1. Ignoring...").
                arg(QString::fromStdString(notification.oasisWsnB2__ProducerReference->Address->__item)), cl_logWARNING );
            continue;
        }

        //TODO/IMPL get relay token and state from notification
        it->second->notificationReceived( notification );
    }

    return SOAP_OK;
}

//!Pass notifications from address \a notificationProducerAddress to \a resource
void OnvifNotificationConsumer::registerResource(
    QnPlOnvifResource* const resource,
    const QString& notificationProducerAddress )
{
    QMutexLocker lk( &m_mutex );
    m_notificationProducerAddressToResource[notificationProducerAddress] = resource;
}

//!Cancel registration of \a resource
void OnvifNotificationConsumer::removeResourceRegistration( QnPlOnvifResource* const resource )
{
    QMutexLocker lk( &m_mutex );

    for( map<QString, QnPlOnvifResource*>::iterator
        it = m_notificationProducerAddressToResource.begin();
        it != m_notificationProducerAddressToResource.end();
         )
    {
        if( it->second == resource )
            it = m_notificationProducerAddressToResource.erase( it );
        else
            ++it;
    }
}

SOAP_SOCKET OnvifNotificationConsumer::accept()
{
    return TCPServerSocket::accept(soap->master);
}
