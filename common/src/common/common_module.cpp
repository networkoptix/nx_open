#include "common_module.h"

#include <QSettings>
#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

#include <api/global_settings.h>
#include "api/runtime_info_manager.h"

#include <common/common_meta_types.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_history.h>
#include "core/resource/camera_user_attribute_pool.h"
#include "core/resource/media_server_user_attributes.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"
#include "core/resource_management/server_additional_addresses_dictionary.h"

#include "utils/common/synctime.h"
#include <utils/common/app_info.h>

#include <nx/network/socket_global.h>

#include <nx/utils/timer_manager.h>
#include <api/http_client_pool.h>


namespace
{
    static const QString kAdminPasswordHash = lit("adminMd5Hash");
    static const QString kAdminPasswordDigest = lit("adminMd5Digest");
    static const QString kAdminPasswordCrypt512 = lit("adminCrypt512");
    static const QString kAdminPasswordRealm = lit("adminRealm");
}

void AdminPasswordData::saveToSettings(QSettings* settings)
{
    settings->setValue(kAdminPasswordHash, hash);
    settings->setValue(kAdminPasswordDigest, digest);
    settings->setValue(kAdminPasswordCrypt512, cryptSha512Hash);
    settings->setValue(kAdminPasswordRealm, realm);
}

void AdminPasswordData::loadFromSettings(const QSettings* settings)
{
    hash = settings->value(kAdminPasswordHash).toByteArray();
    digest = settings->value(kAdminPasswordDigest).toByteArray();
    cryptSha512Hash = settings->value(kAdminPasswordCrypt512).toByteArray();
    realm = settings->value(kAdminPasswordRealm, QnAppInfo::realm()).toByteArray();
}

void AdminPasswordData::clearSettings(QSettings* settings)
{
    settings->remove(kAdminPasswordHash);
    settings->remove(kAdminPasswordDigest);
    settings->remove(kAdminPasswordCrypt512);
    settings->remove(kAdminPasswordRealm);
}

bool AdminPasswordData::isEmpty() const
{
    return digest.isEmpty() && hash.isEmpty();
}

// ------------------- QnCommonModule --------------------

QnCommonModule::QnCommonModule(QObject *parent):
    QObject(parent)
{
    Q_INIT_RESOURCE(common);

    nx::network::SocketGlobals::init();

    m_dirty = false;
    m_cloudMode = false;
    m_engineVersion = QnSoftwareVersion(QnAppInfo::engineVersion());

    QnCommonMetaTypes::initialize();

    /* Init statics. */
    store<nx::utils::TimerManager>(new nx::utils::TimerManager());

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"), true);
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"), false);

    instance<QnSyncTime>();
    instance<QnCameraUserAttributePool>();
    instance<QnMediaServerUserAttributesPool>();
    instance<QnResourcePropertyDictionary>();
    instance<QnResourceStatusDictionary>();
    instance<QnServerAdditionalAddressesDictionary>();

    instance<QnResourcePool>();
    instance<QnResourceAccessManager>();

    instance<QnGlobalSettings>();
    instance<nx_http::ClientPool>();

    /* Init members. */
    m_runUuid = QnUuid::createUuid();
    m_transcodingDisabled = false;
    m_systemIdentityTime = 0;
    m_lowPriorityAdminPassword = false;
    m_localPeerType = Qn::PT_NotDefined;
}

QnCommonModule::~QnCommonModule()
{
    /* Here all singletons will be destroyed, so we guarantee all socket work will stop. */
    clear();

    nx::network::SocketGlobals::deinit();
}

void QnCommonModule::bindModuleinformation(const QnMediaServerResourcePtr &server)
{
    /* Can't use resourceChanged signal because it's not emited when we are saving server locally. */
    connect(server.data(),  &QnMediaServerResource::nameChanged,    this,   &QnCommonModule::resetCachedValue);
    connect(server.data(),  &QnMediaServerResource::apiUrlChanged,  this,   &QnCommonModule::resetCachedValue);
    connect(server.data(),  &QnMediaServerResource::serverFlagsChanged,  this,   &QnCommonModule::resetCachedValue);

    connect(qnGlobalSettings, &QnGlobalSettings::systemNameChanged, this, &QnCommonModule::resetCachedValue);
    connect(qnGlobalSettings, &QnGlobalSettings::localSystemIdChanged, this, &QnCommonModule::resetCachedValue);
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &QnCommonModule::resetCachedValue);

}

void QnCommonModule::setRemoteGUID(const QnUuid &guid)
{
    {
        QnMutexLocker lock( &m_mutex );
        if (m_remoteUuid == guid)
            return;
        m_remoteUuid = guid;
    }
    emit remoteIdChanged(guid);
}

QnUuid QnCommonModule::remoteGUID() const {
    QnMutexLocker lock( &m_mutex );
    return m_remoteUuid;
}

QnMediaServerResourcePtr QnCommonModule::currentServer() const {
    QnUuid serverId = remoteGUID();
    if (serverId.isNull())
        return QnMediaServerResourcePtr();
    return qnResPool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
}

QnSoftwareVersion QnCommonModule::engineVersion() const {
    QnMutexLocker lk( &m_mutex );
    return m_engineVersion;
}

void QnCommonModule::setEngineVersion(const QnSoftwareVersion &version)
{
    QnMutexLocker lk( &m_mutex );
    m_engineVersion = version;
}

void QnCommonModule::setReadOnly(bool value)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_moduleInformation.ecDbReadOnly == value)
            return;
        m_moduleInformation.ecDbReadOnly = value;
    }
    emit moduleInformationChanged();
    emit readOnlyChanged(value);
}

bool QnCommonModule::isReadOnly() const
{
    QnMutexLocker lk(&m_mutex);
    return m_moduleInformation.ecDbReadOnly;
}

void QnCommonModule::setModuleInformation(const QnModuleInformation& moduleInformation)
{
    bool isReadOnlyChanged = false;
    {
        QnMutexLocker lk( &m_mutex );
        if (m_moduleInformation == moduleInformation)
            return;

        isReadOnlyChanged = m_moduleInformation.ecDbReadOnly != moduleInformation.ecDbReadOnly;
        m_moduleInformation = moduleInformation;
    }
    if (isReadOnlyChanged)
        emit readOnlyChanged(moduleInformation.ecDbReadOnly);
    emit moduleInformationChanged();
}

QnModuleInformation QnCommonModule::moduleInformation()
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_dirty)
        {
            updateModuleInformationUnsafe();
            m_dirty = false;
        }
        return m_moduleInformation;
    }

}

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);

    NX_ASSERT(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an NX_ASSERT here? Something is wrong with resource data json file. */
}

void QnCommonModule::resetCachedValue()
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_dirty)
            return;
        m_dirty = true;
    }
    emit moduleInformationChanged();
}

void QnCommonModule::updateModuleInformationUnsafe()
{
    // This code works only on server side.
    NX_ASSERT(!moduleGUID().isNull());

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(moduleGUID());
    NX_ASSERT(server);
    if (!server)
        return;

    m_moduleInformation.port = server->getPort();
    m_moduleInformation.name = server->getName();
    m_moduleInformation.serverFlags = server->getServerFlags();

    m_moduleInformation.systemName = qnGlobalSettings->systemName();
    m_moduleInformation.localSystemId = qnGlobalSettings->localSystemId();
    m_moduleInformation.cloudSystemId = qnGlobalSettings->cloudSystemId();
}

void QnCommonModule::setSystemIdentityTime(qint64 value, const QnUuid& sender)
{
    m_systemIdentityTime = value;
    emit systemIdentityTimeChanged(value, sender);
}

qint64 QnCommonModule::systemIdentityTime() const
{
    return m_systemIdentityTime;
}

void QnCommonModule::setAdminPasswordData(const AdminPasswordData& data)
{
    m_adminPasswordData = data;
}

AdminPasswordData QnCommonModule::adminPasswordData() const
{
    return m_adminPasswordData;
}

void QnCommonModule::setUseLowPriorityAdminPasswordHach(bool value)
{
    m_lowPriorityAdminPassword = value;
}

bool QnCommonModule::useLowPriorityAdminPasswordHach() const
{
    return m_lowPriorityAdminPassword;
}

QnUuid QnCommonModule::runningInstanceGUID() const
{
    QnMutexLocker lock(&m_mutex);
    return m_runUuid;
}

void QnCommonModule::updateRunningInstanceGuid()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_runUuid = QnUuid::createUuid();
    }
    emit runningInstanceGUIDChanged();
}

void QnCommonModule::setLocalPeerType(Qn::PeerType peerType)
{
    m_localPeerType = peerType;
}

Qn::PeerType QnCommonModule::localPeerType() const
{
    return m_localPeerType;
}
