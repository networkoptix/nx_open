#include "media_server_resource.h"

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include "api/session_manager.h"
#include <api/app_server_connection.h>
#include <api/model/ping_reply.h>
#include <api/network_proxy_factory.h>

#include <core/resource/storage_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/server_backup_schedule.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_pool.h>

#include "nx_ec/ec_proto_version.h"

#include <rest/server/json_rest_result.h>

#include <utils/common/app_info.h>
#include "utils/common/delete_later.h"
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include <nx/network/http/asynchttpclient.h>
#include "network/networkoptixmodulerevealcommon.h"
#include "utils/serialization/lexical.h"
#include "api/server_rest_connection.h"

namespace {
    const QString protoVersionPropertyName = lit("protoVersion");
    const QString safeModePropertyName = lit("ecDbReadOnly");

    quint16 portFromUrl(const QString &strUrl)
    {
        const QUrl url(strUrl);
        return url.port(DEFAULT_APPSERVER_PORT);
    }
}


class QnMediaServerResourceGuard: public QObject {
public:
    QnMediaServerResourceGuard(const QnMediaServerResourcePtr &resource): m_resource(resource) {}

private:
    QnMediaServerResourcePtr m_resource;
};

QnMediaServerResource::QnMediaServerResource(const QnResourceTypePool* resTypePool):
    m_serverFlags(Qn::SF_None),
    m_panicModeCache(
        std::bind(&QnMediaServerResource::calculatePanicMode, this),
        &m_mutex )
{
    setTypeId(resTypePool->getFixedResourceTypeId(QnResourceTypePool::kServerTypeId));
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #Elric is this call needed here?

    //TODO: #GDM #EDGE in case of EDGE servers getName should return name of its camera. Possibly name just should be synced on Server.
    QnResource::setName(tr("Server"));

    m_statusTimer.restart();

    QnResourceList resList = qnResPool->getResourcesByParentId(getId()).filtered<QnSecurityCamResource>();
    if (!resList.isEmpty())
        m_firstCamera = resList.first();

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnMediaServerResource::onNewResource, Qt::DirectConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnMediaServerResource::onRemoveResource, Qt::DirectConnection);
    connect(this, &QnResource::resourceChanged, this, &QnMediaServerResource::atResourceChanged, Qt::DirectConnection);
    connect(this, &QnResource::propertyChanged, this, &QnMediaServerResource::at_propertyChanged, Qt::DirectConnection);
}

QnMediaServerResource::~QnMediaServerResource()
{
    QnMutexLocker lock( &m_mutex );
    m_runningIfRequests.clear();
}

void QnMediaServerResource::at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == QnMediaResource::panicRecordingKey())
        m_panicModeCache.update();
}

void QnMediaServerResource::onNewResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock( &m_mutex );
    if (m_firstCamera.isNull() && resource.dynamicCast<QnSecurityCamResource>() &&  resource->getParentId() == getId())
        m_firstCamera = resource;
}

void QnMediaServerResource::onRemoveResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock( &m_mutex );
    if (m_firstCamera && resource->getId() == m_firstCamera->getId())
        m_firstCamera.clear();
}

void QnMediaServerResource::beforeDestroy()
{
    QnMutexLocker lock(&m_mutex);
    m_firstCamera.clear();
}

void QnMediaServerResource::atResourceChanged()
{
    m_panicModeCache.update();
}

QString QnMediaServerResource::getUniqueId() const
{
    assert(!getId().isNull());
    return QLatin1String("Server ") + getId().toString();
}

QString QnMediaServerResource::getName() const
{
    if (getServerFlags() & Qn::SF_Edge)
    {
        QnMutexLocker lock( &m_mutex );
        if (m_firstCamera)
            return m_firstCamera->getName();
    }

    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if( !(*lk)->name.isEmpty() )
            return (*lk)->name;
    }
    return QnResource::getName();
}

void QnMediaServerResource::setName( const QString& name )
{
    if (getId().isNull())
        return;

    if (getServerFlags() & Qn::SF_Edge)
        return;

    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->name == name)
            return;
        (*lk)->name = name;
    }
    emit nameChanged(toSharedPointer(this));
}


void QnMediaServerResource::setApiUrl(const QString &apiUrl)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (apiUrl == m_apiUrl)
            return;

        m_apiUrl = apiUrl;
        if (m_apiConnection)
            m_apiConnection->setUrl(m_apiUrl);
    }
    emit apiUrlChanged(::toSharedPointer(this));
}

QString QnMediaServerResource::getApiUrl() const
{
    QnMutexLocker lock( &m_mutex );
    return m_apiUrl;
}

void QnMediaServerResource::setNetAddrList(const QList<SocketAddress>& netAddrList)
{
    QnMutexLocker lock( &m_mutex );
    m_netAddrList = netAddrList;
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<SocketAddress> QnMediaServerResource::getNetAddrList() const
{
    QnMutexLocker lock( &m_mutex );
    return m_netAddrList;
}

void QnMediaServerResource::setAdditionalUrls(const QList<QUrl> &urls)
{
    QnUuid id = getId();
    QList<QUrl> oldUrls = QnServerAdditionalAddressesDictionary::instance()->additionalUrls(id);
    if (oldUrls == urls)
        return;

    QnServerAdditionalAddressesDictionary::instance()->setAdditionalUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getAdditionalUrls() const
{
    return QnServerAdditionalAddressesDictionary::instance()->additionalUrls(getId());
}

void QnMediaServerResource::setIgnoredUrls(const QList<QUrl> &urls)
{
    QnUuid id = getId();
    QList<QUrl> oldUrls = QnServerAdditionalAddressesDictionary::instance()->ignoredUrls(id);
    if (oldUrls == urls)
        return;

    QnServerAdditionalAddressesDictionary::instance()->setIgnoredUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getIgnoredUrls() const
{
    return QnServerAdditionalAddressesDictionary::instance()->ignoredUrls(getId());
}

quint16 QnMediaServerResource::getPort() const {
    return portFromUrl(getApiUrl());
}

QnMediaServerConnectionPtr QnMediaServerResource::apiConnection()
{
    QnMutexLocker lock( &m_mutex );

    /* We want the video server connection to be deleted in its associated thread,
     * no matter where the reference count reached zero. Hence the custom deleter. */
    if (!m_apiConnection && !m_apiUrl.isEmpty())
        m_apiConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(this, QnAppServerConnectionFactory::videowallGuid()), &qnDeleteLater);

    return m_apiConnection;
}

rest::QnConnectionPtr QnMediaServerResource::restConnection()
{
    QnMutexLocker lock( &m_mutex );

    if (!m_restConnection)
        m_restConnection = rest::QnConnectionPtr(new rest::ServerConnection(getId()));

    return m_restConnection;
}

QnResourcePtr QnMediaServerResourceFactory::createResource(const QnUuid& resourceTypeId, const QnResourceParams& /*params*/)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnMediaServerResource(qnResTypePool));
    //result->deserialize(parameters);

    return result;
}

QnStorageResourceList QnMediaServerResource::getStorages() const
{
    return qnResPool->getResourcesByParentId(getId()).filtered<QnStorageResource>();
}

void QnMediaServerResource::setPrimaryAddress(const SocketAddress& primaryAddress)
{
    QString apiScheme = QUrl(getApiUrl()).scheme();
    QString urlScheme = QUrl(getUrl()).scheme();
    setApiUrl(lit("%1://%2").arg(apiScheme).arg(primaryAddress.toString()));
    setUrl(lit("%1://%2").arg(urlScheme).arg(primaryAddress.toString()));
}

Qn::PanicMode QnMediaServerResource::getPanicMode() const
{
    return m_panicModeCache.get();
}

Qn::PanicMode QnMediaServerResource::calculatePanicMode() const
{
    QString strVal = getProperty(QnMediaResource::panicRecordingKey());
    Qn::PanicMode result = Qn::PM_None;
    QnLexical::deserialize(strVal, &result);
    return result;
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode) {
    if(getPanicMode() == panicMode)
        return;
    QString strVal;
    QnLexical::serialize(panicMode, &strVal);
    setProperty(QnMediaResource::panicRecordingKey(), strVal);
    m_panicModeCache.update();
}

Qn::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(Qn::ServerFlags flags)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (flags == m_serverFlags)
            return;
        m_serverFlags = flags;
    }
    emit serverFlagsChanged(::toSharedPointer(this));
}

QnStorageResourcePtr QnMediaServerResource::getStorageByUrl(const QString& url) const
{
   for(const QnStorageResourcePtr& storage: getStorages()) {
       if (storage->getUrl() == url)
           return storage;
   }
   return QnStorageResourcePtr();
}

void QnMediaServerResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) {
    QString oldUrl = m_url;
    QnResource::updateInner(other, modifiedFields);
    bool netAddrListChanged = false;

    QnMediaServerResource* localOther = dynamic_cast<QnMediaServerResource*>(other.data());
    if(localOther) {
        if (m_version != localOther->m_version)
            modifiedFields << "versionChanged";

        m_serverFlags = localOther->m_serverFlags;
        netAddrListChanged = m_netAddrList != localOther->m_netAddrList;
        m_netAddrList = localOther->m_netAddrList;
        m_version = localOther->m_version;
        m_systemInfo = localOther->m_systemInfo;
        m_systemName = localOther->m_systemName;

        /*
        QnAbstractStorageResourceList otherStorages = localOther->getStorages();

        // Keep indices unchanged (Server does not provide this info).
        for(const QnAbstractStorageResourcePtr &storage: m_storages)
        {
            for(const QnAbstractStorageResourcePtr &otherStorage: otherStorages)
            {
                if (otherStorage->getId() == storage->getId()) {
                    otherStorage->setIndex(storage->getIndex());
                    break;
                }
            }
        }

        setStorages(otherStorages);
        */
    }


    const bool currentPortChanged = (portFromUrl(m_apiUrl) != localOther->getPort());
    if (netAddrListChanged || currentPortChanged )
    {
        m_apiUrl = localOther->m_apiUrl;    // do not update autodetected value with side changes
        if (m_apiConnection)
            m_apiConnection->setUrl(m_apiUrl);
        if (currentPortChanged )
            modifiedFields << "portChanged";
    } else {
        m_url = oldUrl; //rollback changed value to autodetected
    }
}

QnSoftwareVersion QnMediaServerResource::getVersion() const
{
    QnMutexLocker lock( &m_mutex );

    return m_version;
}

void QnMediaServerResource::setMaxCameras(int value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    (*lk)->maxCameras = value;
}

int QnMediaServerResource::getMaxCameras() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->maxCameras;
}

QnServerBackupSchedule QnMediaServerResource::getBackupSchedule() const {
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->backupSchedule;
}

void QnMediaServerResource::setBackupSchedule(const QnServerBackupSchedule &value) {
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->backupSchedule == value)
            return;
        (*lk)->backupSchedule = value;
    }
    emit backupScheduleChanged(::toSharedPointer(this));
}

void QnMediaServerResource::setRedundancy(bool value)
{
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->isRedundancyEnabled == value)
            return;
        (*lk)->isRedundancyEnabled = value;
    }
    emit redundancyChanged(::toSharedPointer(this));
}

bool QnMediaServerResource::isRedundancy() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->isRedundancyEnabled;
}

void QnMediaServerResource::setVersion(const QnSoftwareVersion &version)
{
    {
        QnMutexLocker lock( &m_mutex );
        if (m_version == version)
            return;
        m_version = version;
    }
    emit versionChanged(::toSharedPointer(this));
}

QnSystemInformation QnMediaServerResource::getSystemInfo() const {
    QnMutexLocker lock( &m_mutex );

    return m_systemInfo;
}

void QnMediaServerResource::setSystemInfo(const QnSystemInformation &systemInfo) {
    QnMutexLocker lock( &m_mutex );

    m_systemInfo = systemInfo;
}

QString QnMediaServerResource::getSystemName() const {
    QnMutexLocker lock( &m_mutex );

    return m_systemName;
}

void QnMediaServerResource::setSystemName(const QString &systemName) {
    {
        QnMutexLocker lock( &m_mutex );

        if (m_systemName == systemName)
            return;

        m_systemName = systemName;
    }
    emit systemNameChanged(toSharedPointer(this));
}

QnModuleInformation QnMediaServerResource::getModuleInformation() const {
    QnModuleInformation moduleInformation;
    moduleInformation.type = QnModuleInformation::nxMediaServerId();
    moduleInformation.customization = QnAppInfo::customizationName();
    moduleInformation.sslAllowed = false;
    moduleInformation.protoVersion = getProperty(protoVersionPropertyName).toInt();
    moduleInformation.name = getName();
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;

    if (hasProperty(safeModePropertyName))
        moduleInformation.ecDbReadOnly = QnLexical::deserialized(getProperty(safeModePropertyName), moduleInformation.ecDbReadOnly);

    QnMutexLocker lock( &m_mutex );

    moduleInformation.version = m_version;
    moduleInformation.systemInformation = m_systemInfo;
    moduleInformation.systemName = m_systemName;
    moduleInformation.port = QUrl(m_apiUrl).port();
    moduleInformation.id = getId();
    moduleInformation.serverFlags = getServerFlags();

    return moduleInformation;
}

void QnMediaServerResource::setModuleInformation(const QnModuleInformationWithAddresses &moduleInformation) {
    Q_ASSERT_X(isFakeServer(toSharedPointer()), Q_FUNC_INFO, "Only fake servers should be set this way");

    QList<SocketAddress> addressList;
    for (const QString &address: moduleInformation.remoteAddresses)
        addressList.append(SocketAddress(address));
    setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString address = addressList.first().toString();
        quint16 port = moduleInformation.port;
        QString url = QString(lit("http://%1:%2")).arg(address).arg(port);
        setApiUrl(url);
        setUrl(url);
    }
    if (!moduleInformation.name.isEmpty())
        setName(moduleInformation.name);
    setVersion(moduleInformation.version);
    setSystemInfo(moduleInformation.systemInformation);
    setSystemName(moduleInformation.systemName);
    setProperty(protoVersionPropertyName, QString::number(moduleInformation.protoVersion));
    setProperty(safeModePropertyName, QnLexical::serialized(moduleInformation.ecDbReadOnly));

    if (moduleInformation.ecDbReadOnly)
        addFlags(Qn::read_only);
    else
        removeFlags(Qn::read_only);
}

bool QnMediaServerResource::isEdgeServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags() & Qn::SF_Edge);
    return false;
}

bool QnMediaServerResource::isHiddenServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags() & Qn::SF_Edge) && !server->isRedundancy();
    return false;
}

QnUuid QnMediaServerResource::getOriginalGuid() const {
    QnMutexLocker lock(&m_mutex);
    return m_originalGuid;
}

void QnMediaServerResource::setOriginalGuid(const QnUuid &guid) {
    QnMutexLocker lock(&m_mutex);
    m_originalGuid = guid;
}

bool QnMediaServerResource::isFakeServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return !server->getOriginalGuid().isNull();
    return false;
}

void QnMediaServerResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    if (getStatus() != newStatus)
    {
        {
            QnMutexLocker lock( &m_mutex );
            m_statusTimer.restart();
            Q_ASSERT_X(newStatus == Qn::Incompatible || newStatus == Qn::Unauthorized || m_originalGuid.isNull(),
                       Q_FUNC_INFO,
                       "Incompatible servers should not take any status but incompatible or unauthorized");
        }

        QnResource::setStatus(newStatus, silenceMode);
        if (!silenceMode)
        {
            QnResourceList childList = qnResPool->getResourcesByParentId(getId());
            for(const QnResourcePtr& res: childList)
            {
                if (res->hasFlags(Qn::depend_on_parent_status))
                    emit res->statusChanged(res);
            }
        }
    }
}

qint64 QnMediaServerResource::currentStatusTime() const
{
    QnMutexLocker lock( &m_mutex );
    return m_statusTimer.elapsed();
}

QString QnMediaServerResource::getAuthKey() const
{
    return m_authKey;
}

void QnMediaServerResource::setAuthKey(const QString& authKey)
{
    m_authKey = authKey;
}

QString QnMediaServerResource::realm() const
{
    return QnAppInfo::realm();
}
