#include "app_server_connection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include <business/business_event_rule.h>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource/user_resource.h"

#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/common/model_functions.h>
#include <utils/common/synctime.h>

#include "session_manager.h"
#include "common_message_processor.h"


// -------------------------------------------------------------------------- //
// QnAppServerConnectionFactory
// -------------------------------------------------------------------------- //
Q_GLOBAL_STATIC(QnAppServerConnectionFactory, qn_appServerConnectionFactory_instance)

QnAppServerConnectionFactory::QnAppServerConnectionFactory()
{}

QnAppServerConnectionFactory::~QnAppServerConnectionFactory() {
    return;
}

void QnAppServerConnectionFactory::setCurrentVersion(const QnSoftwareVersion &version)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_currentVersion = version;
    }
}

QnSoftwareVersion QnAppServerConnectionFactory::currentVersion()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_currentVersion;
    }

    return QnSoftwareVersion();
}

QnResourceFactory* QnAppServerConnectionFactory::defaultFactory()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_resourceFactory;
    }

    return 0;
}

QString QnAppServerConnectionFactory::clientGuid()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_clientGuid;
    }

    return QString();
}

QUrl QnAppServerConnectionFactory::url() {
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        Q_ASSERT_X(factory->m_url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url was requested");

        QMutexLocker locker(&factory->m_mutex);
        return factory->m_url;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setClientGuid(const QString &guid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_clientGuid = guid;
    }
}


void QnAppServerConnectionFactory::setUrl(const QUrl &url) {
    if (url.isValid())
        Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        QMutexLocker locker(&factory->m_mutex);
        factory->m_url = url;
    }
}

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_resourceFactory = resourceFactory;
    }
}

QnUuid QnAppServerConnectionFactory::videowallGuid() {
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_videowallGuid;
    return QnUuid();
}

void QnAppServerConnectionFactory::setVideowallGuid(const QnUuid &uuid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_videowallGuid = uuid;
    }
}

QnUuid QnAppServerConnectionFactory::instanceGuid() {
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_instanceGuid;
    return QnUuid();
}

void QnAppServerConnectionFactory::setInstanceGuid(const QnUuid &uuid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_instanceGuid = uuid;
    }
}

static ec2::AbstractECConnectionFactory* ec2ConnectionFactoryInstance = nullptr;

void QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* _ec2ConnectionFactory )
{
    ec2ConnectionFactoryInstance = _ec2ConnectionFactory;
}

ec2::AbstractECConnectionFactory* QnAppServerConnectionFactory::ec2ConnectionFactory()
{
    return ec2ConnectionFactoryInstance;
}

static ec2::AbstractECConnectionPtr currentlyUsedEc2Connection;
void QnAppServerConnectionFactory::setEc2Connection(const ec2::AbstractECConnectionPtr &ec2Connection )
{
    currentlyUsedEc2Connection = ec2Connection;
}

ec2::AbstractECConnectionPtr QnAppServerConnectionFactory::getConnection2() {
    return currentlyUsedEc2Connection;
}

bool initResourceTypes(ec2::AbstractECConnectionPtr ec2Connection) // TODO: #Elric #EC2 pass reference
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager()->getResourceTypesSync(&resourceTypeList);
    if( errorCode != ec2::ErrorCode::ok )
    {
        NX_LOG( QString::fromLatin1("Failed to load resource types. %1").arg(ec2::toString(errorCode)), cl_logERROR );
        return false;
    }

    qnResTypePool->replaceResourceTypeList(resourceTypeList);
    return true;
}
