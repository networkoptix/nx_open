#include "common_module.h"

#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <api/session_manager.h>
#include <common/common_meta_types.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
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
}

QnCommonModule::~QnCommonModule() {
    delete m_sessionManager;
    return;
}

void QnCommonModule::setLocalSystemName(const QString &value) {
    if (m_localSystemName == value)
        return;

    {
        QMutexLocker lk(&m_mutex);
        m_localSystemName = value;
    }
    emit systemNameChanged(m_localSystemName);
}

QString QnCommonModule::localSystemName() const
{
    QMutexLocker lk(&m_mutex);
    return m_localSystemName;
}

QnSoftwareVersion QnCommonModule::engineVersion() const {
    QMutexLocker lk(&m_mutex);
    return m_engineVersion;
}

void QnCommonModule::setEngineVersion(const QnSoftwareVersion &version) {
    QMutexLocker lk(&m_mutex);
    m_engineVersion = version;
}

void QnCommonModule::setModuleInformation(const QnModuleInformation &moduleInformation) {
    QMutexLocker lk(&m_mutex);
    m_moduleInformation = moduleInformation;
}

QnModuleInformation QnCommonModule::moduleInformation() const 
{
    QnModuleInformation moduleInformationCopy;
    {
        QMutexLocker lk(&m_mutex);
        moduleInformationCopy = m_moduleInformation;
        moduleInformationCopy.systemName = m_localSystemName;
    }
    //filling dynamic fields
    if (qnResPool) {
        moduleInformationCopy.remoteAddresses.clear();
        const QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
        if (server) {
            foreach(const QHostAddress &address, server->getNetAddrList())
                moduleInformationCopy.remoteAddresses.insert(address.toString());
        }
    }

    return moduleInformationCopy;
}

void QnCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);
    
    Q_ASSERT_X(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an assert here? Something is wrong with resource data json file. */
}
