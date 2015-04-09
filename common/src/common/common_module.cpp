#include "common_module.h"

#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

#include <api/session_manager.h>
#include <common/common_meta_types.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_history.h>
#include <utils/common/product_features.h>


QnCommonModule::QnCommonModule(int &, char **, QObject *parent): QObject(parent) {
    Q_INIT_RESOURCE(common);
    m_cloudMode = false;
    m_engineVersion = QnSoftwareVersion(QnAppInfo::engineVersion());

    QnCommonMetaTypes::initialize();
    
    /* Init statics. */
    qnProductFeatures();

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"), true);
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"), false);

    instance<QnResourcePool>();
    instance<QnCameraHistoryPool>();

    /* Init members. */
    m_sessionManager = new QnSessionManager(); //instance<QnSessionManager>();
    m_runUuid = QnUuid::createUuid();
    m_transcodingDisabled = false;
    m_systemIdentityTime = 0;
    m_lowPriorityAdminPassword = false;
}

QnCommonModule::~QnCommonModule() {
    delete m_sessionManager;
}

void QnCommonModule::bindModuleinformation(const QnMediaServerResourcePtr &server) {
    /* Can't use resourceChanged signal because it's not emited when we are saving server locally. */
    connect(server.data(),  &QnMediaServerResource::nameChanged,    this,   &QnCommonModule::updateModuleInformation);
    connect(server.data(),  &QnMediaServerResource::apiUrlChanged,  this,   &QnCommonModule::updateModuleInformation);
}

void QnCommonModule::bindModuleinformation(const QnUserResourcePtr &adminUser) {
    connect(adminUser.data(),   &QnUserResource::resourceChanged,   this,   &QnCommonModule::updateModuleInformation);
    connect(adminUser.data(),   &QnUserResource::hashChanged,       this,   &QnCommonModule::updateModuleInformation);
}

void QnCommonModule::setRemoteGUID(const QnUuid &guid) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_remoteUuid == guid)
            return;
        m_remoteUuid = guid;
    }
    emit remoteIdChanged(guid);
}

QnUuid QnCommonModule::remoteGUID() const {
    QMutexLocker lock(&m_mutex);
    return m_remoteUuid;
}

QnMediaServerResourcePtr QnCommonModule::currentServer() const {
    QnUuid serverId = remoteGUID();
    if (serverId.isNull())
        return QnMediaServerResourcePtr();
    return qnResPool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
}

QnSoftwareVersion QnCommonModule::engineVersion() const {
    QMutexLocker lk(&m_mutex);
    return m_engineVersion;
}

void QnCommonModule::setEngineVersion(const QnSoftwareVersion &version) {
    QMutexLocker lk(&m_mutex);
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

void QnCommonModule::setModuleInformation(const QnModuleInformation &moduleInformation)
{
    bool isSystemNameChanged = false;
    {
        QMutexLocker lk(&m_mutex);
        if (m_moduleInformation == moduleInformation)
            return;

        isSystemNameChanged = m_moduleInformation.systemName != moduleInformation.systemName;
        m_moduleInformation = moduleInformation;
    }
    if (isSystemNameChanged)
        emit systemNameChanged(moduleInformation.systemName);
    emit moduleInformationChanged();
}

QnModuleInformation QnCommonModule::moduleInformation() const
{
    QMutexLocker lk(&m_mutex);
    return m_moduleInformation;
}

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);
    
    Q_ASSERT_X(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an assert here? Something is wrong with resource data json file. */
}

void QnCommonModule::updateModuleInformation() {
    QMutexLocker lk(&m_mutex);
    QnModuleInformation moduleInformationCopy = m_moduleInformation;
    lk.unlock();

    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (server) {
        QnModuleInformation moduleInformation = server->getModuleInformation();
        moduleInformationCopy.port = moduleInformation.port;
        moduleInformationCopy.name = moduleInformation.name;
    }

    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (admin) {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(admin->getHash());
        md5.addData(moduleInformationCopy.systemName.toUtf8());
        moduleInformationCopy.authHash = md5.result();
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
