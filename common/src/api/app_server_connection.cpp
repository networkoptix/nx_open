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

#include "utils/common/sleep.h"
#include <utils/common/model_functions.h>
#include "utils/common/synctime.h"
#include "session_manager.h"
#include "common_message_processor.h"


// -------------------------------------------------------------------------- //
// QnAppServerConnectionFactory
// -------------------------------------------------------------------------- //
Q_GLOBAL_STATIC(QnAppServerConnectionFactory, qn_appServerConnectionFactory_instance)

QnAppServerConnectionFactory::QnAppServerConnectionFactory(): 
    m_prematureLicenseExperationDate(0)
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

QByteArray QnAppServerConnectionFactory::prevSessionKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_prevSessionKey;
    return QByteArray();
}


QByteArray QnAppServerConnectionFactory::sessionKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
            return factory->m_sessionKey;
    return QByteArray();
}

void QnAppServerConnectionFactory::setSessionKey(const QByteArray& sessionKey)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        if (sessionKey != factory->m_sessionKey) {
            factory->m_prevSessionKey = factory->m_sessionKey;
            factory->m_sessionKey = sessionKey.trimmed();
        }
    }
}

qint64 QnAppServerConnectionFactory::prematureLicenseExperationDate()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_prematureLicenseExperationDate;
    return 0;
}

void QnAppServerConnectionFactory::setPrematureLicenseExperationDate(qint64 value)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        if (value != factory->m_prematureLicenseExperationDate) {
            factory->m_prematureLicenseExperationDate = value;
        }
    }
}

void QnAppServerConnectionFactory::setPublicIp(const QString &publicIp)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_publicUrl.setHost(publicIp);
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

QString QnAppServerConnectionFactory::authKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_authKey;
    }

    return QString();
}

QString QnAppServerConnectionFactory::box()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_box;
    }

    return QString();
}

QString QnAppServerConnectionFactory::clientGuid()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_clientGuid;
    }

    return QString();
}

QUrl QnAppServerConnectionFactory::defaultUrl()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_defaultUrl;
    }

    return QUrl();
}

QUrl QnAppServerConnectionFactory::publicUrl()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_publicUrl;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setBox(const QString &box)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_box = box;
    }
}

void QnAppServerConnectionFactory::setAuthKey(const QString &authKey)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_authKey = authKey;
    }
}

void QnAppServerConnectionFactory::setClientGuid(const QString &guid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_clientGuid = guid;
    }
}

void QnAppServerConnectionFactory::setDefaultUrl(const QUrl &url)
{
    Q_ASSERT_X(url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url was passed");
    Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_defaultUrl = url;
        factory->m_publicUrl = url;
    }
}

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_resourceFactory = resourceFactory;
    }
}

QUuid QnAppServerConnectionFactory::videowallGuid() {
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_videowallGuid;
    return QUuid();
}

void QnAppServerConnectionFactory::setVideowallGuid(const QUuid &uuid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_videowallGuid = uuid;
    }
}

QUuid QnAppServerConnectionFactory::instanceGuid() {
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_instanceGuid;
    return QUuid();
}

void QnAppServerConnectionFactory::setInstanceGuid(const QUuid &uuid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_instanceGuid = uuid;
    }
}

//QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(const QUrl& url)
//{
//    QUrl urlNoPassword (url);
//    urlNoPassword.setPassword(QString());
//
//    NX_LOG(QLatin1String("Creating connection to the Enterprise Controller ") + urlNoPassword.toString(), cl_logDEBUG2);
//
//    return QnAppServerConnectionPtr(new QnAppServerConnection(
//        url,
//        *(qn_appServerConnectionFactory_instance()->m_resourceFactory),
//        qn_appServerConnectionFactory_instance()->m_serializer,
//        qn_appServerConnectionFactory_instance()->m_clientGuid,
//        qn_appServerConnectionFactory_instance()->m_authKey)
//    );
//}
//
//QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection()
//{
//    return createConnection(defaultUrl());
//}

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
void QnAppServerConnectionFactory::setEc2Connection( ec2::AbstractECConnectionPtr ec2Connection )
{
    currentlyUsedEc2Connection = ec2Connection;
}

ec2::AbstractECConnectionPtr QnAppServerConnectionFactory::getConnection2()
{
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
