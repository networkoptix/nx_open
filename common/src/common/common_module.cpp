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

    /* Init members. */
    m_sessionManager = new QnSessionManager(); //instance<QnSessionManager>();
    m_runUuid = QnUuid::createUuid();
    m_transcodingDisabled = false;
    m_systemIdentityTime = 0;
    m_lowPriorityAdminPassword = false;
}

QnCommonModule::~QnCommonModule() {
    delete m_sessionManager;
    return;
}

void QnCommonModule::setRemoteGUID(const QnUuid &guid) {
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        if (m_remoteUuid == guid)
            return;
        m_remoteUuid = guid;
    }
    emit remoteIdChanged(guid);
}

QnUuid QnCommonModule::remoteGUID() const {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_remoteUuid;
}

QnSoftwareVersion QnCommonModule::engineVersion() const {
    SCOPED_MUTEX_LOCK( lk, &m_mutex );
    return m_engineVersion;
}

void QnCommonModule::setEngineVersion(const QnSoftwareVersion &version) {
    SCOPED_MUTEX_LOCK( lk, &m_mutex );
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
        SCOPED_MUTEX_LOCK( lk, &m_mutex );
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
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    return m_moduleInformation;
}

/*
QnModuleInformation QnCommonModule::moduleInformation() const
{
    QnModuleInformation moduleInformationCopy;
    {
        SCOPED_MUTEX_LOCK( lk, &m_mutex);
        moduleInformationCopy = m_moduleInformation;
    }
    moduleInformationCopy.runtimeId = runningInstanceGUID();
    //filling dynamic fields
    if (qnResPool) {
        moduleInformationCopy.remoteAddresses.clear();
        const QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
        if (server) {
            QSet<QString> ignoredHosts;
            for (const QUrl &url: server->getIgnoredUrls())
                ignoredHosts.insert(url.host());

            for(const QHostAddress &address: server->getNetAddrList()) {
                QString addressString = address.toString();
                if (!ignoredHosts.contains(addressString))
                    moduleInformationCopy.remoteAddresses.insert(addressString);
            }
            for(const QUrl &url: server->getAdditionalUrls()) {
                if (!ignoredHosts.contains(url.host()))
                    moduleInformationCopy.remoteAddresses.insert(url.host());
            }
            moduleInformationCopy.port = server->getPort();
            moduleInformationCopy.name = server->getName();
        }

        QnUserResourcePtr admin = qnResPool->getAdministrator();
        if (admin) {
            QCryptographicHash md5(QCryptographicHash::Md5);
            md5.addData(admin->getHash());
            md5.addData(moduleInformationCopy.systemName.toUtf8());
            moduleInformationCopy.authHash = md5.result();
        }

    }

    return moduleInformationCopy;
}
*/

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);
    
    Q_ASSERT_X(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an assert here? Something is wrong with resource data json file. */
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
