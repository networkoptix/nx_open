#include "common_module.h"

#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

#include <common/common_meta_types.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_history.h>
#include <utils/common/product_features.h>
#include "core/resource/camera_user_attribute_pool.h"
#include "core/resource/media_server_user_attributes.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"
#include "core/resource_management/server_additional_addresses_dictionary.h"
#include "utils/common/synctime.h"
#include "api/runtime_info_manager.h"

#include <nx/network/socket_global.h>

#include <nx/utils/timermanager.h>

QnCommonModule::QnCommonModule(QObject *parent): QObject(parent) {
    Q_INIT_RESOURCE(common);

    nx::network::SocketGlobals::init();

    m_cloudMode = false;
    m_engineVersion = QnSoftwareVersion(QnAppInfo::engineVersion());

    QnCommonMetaTypes::initialize();

    /* Init statics. */
    qnProductFeatures();
    store<TimerManager>(new TimerManager());

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

void QnCommonModule::bindModuleinformation(const QnMediaServerResourcePtr &server) {
    /* Can't use resourceChanged signal because it's not emited when we are saving server locally. */
    connect(server.data(),  &QnMediaServerResource::nameChanged,    this,   &QnCommonModule::updateModuleInformation);
    connect(server.data(),  &QnMediaServerResource::apiUrlChanged,  this,   &QnCommonModule::updateModuleInformation);
    connect(server.data(),  &QnMediaServerResource::serverFlagsChanged,  this,   &QnCommonModule::updateModuleInformation);
}

void QnCommonModule::setRemoteGUID(const QnUuid &guid) {
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

void QnCommonModule::setEngineVersion(const QnSoftwareVersion &version) {
    QnMutexLocker lk( &m_mutex );
    m_engineVersion = version;
}

void QnCommonModule::setLocalSystemName(const QString& value)
{
    QnModuleInformation info = moduleInformation();
    info.systemName = value;
    setModuleInformation(info);
}

QString QnCommonModule::localSystemName() const
{
    return moduleInformation().systemName;
}

void QnCommonModule::setReadOnly(bool value) {
    QnModuleInformation info = moduleInformation();
    info.ecDbReadOnly = value;
    setModuleInformation(info);
}

bool QnCommonModule::isReadOnly() const {
    return moduleInformation().ecDbReadOnly;
}

void QnCommonModule::setModuleInformation(const QnModuleInformation &moduleInformation)
{
    bool isSystemNameChanged = false;
    bool isReadOnlyChanged = false;
    {
        QnMutexLocker lk( &m_mutex );
        if (m_moduleInformation == moduleInformation)
            return;

        isSystemNameChanged = m_moduleInformation.systemName != moduleInformation.systemName;
        isReadOnlyChanged = m_moduleInformation.ecDbReadOnly != moduleInformation.ecDbReadOnly;
        m_moduleInformation = moduleInformation;
    }
    if (isSystemNameChanged)
        emit systemNameChanged(moduleInformation.systemName);
    if (isReadOnlyChanged)
        emit readOnlyChanged(moduleInformation.ecDbReadOnly);
    emit moduleInformationChanged();
}

QnModuleInformation QnCommonModule::moduleInformation() const
{
    QnMutexLocker lk( &m_mutex );
    return m_moduleInformation;
}

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);

    NX_ASSERT(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an NX_ASSERT here? Something is wrong with resource data json file. */
}

void QnCommonModule::updateModuleInformation() {
    QnMutexLocker lk( &m_mutex );
    QnModuleInformation moduleInformationCopy = m_moduleInformation;
    lk.unlock();

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(moduleGUID());
    if (server) {
        QnModuleInformation moduleInformation = server->getModuleInformation();
        moduleInformationCopy.port = moduleInformation.port;
        moduleInformationCopy.name = moduleInformation.name;
        moduleInformationCopy.serverFlags = moduleInformation.serverFlags;
    }

    setModuleInformation(moduleInformationCopy);
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

void QnCommonModule::setAdminPasswordData(const QByteArray& hash, const QByteArray& digest)
{
    m_adminPaswdHash = hash;
    m_adminPaswdDigest = digest;
}

void QnCommonModule::adminPasswordData(QByteArray* hash, QByteArray* digest) const
{
    *hash = m_adminPaswdHash;
    *digest = m_adminPaswdDigest;
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
