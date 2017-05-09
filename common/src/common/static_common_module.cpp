#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/long_runable_cleanup.h>
#include <api/http_client_pool.h>
#include <utils/common/synctime.h>

#include "common_meta_types.h"

QnStaticCommonModule::QnStaticCommonModule(
    Qn::PeerType localPeerType,
    const QString& brand,
    const QString& customization,
    QObject *parent)
    :
    QObject(parent),
    m_localPeerType(localPeerType),
    m_brand(brand),
    m_customization(customization),
    m_engineVersion(QnAppInfo::engineVersion())
{
    Q_INIT_RESOURCE(common);
    QnCommonMetaTypes::initialize();
    instance<QnLongRunnablePool>();
    nx::network::SocketGlobals::init();

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"), true);
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"), false);

    store(new QnLongRunableCleanup());
    store(new nx::utils::TimerManager());

    instance<QnSyncTime>();
    instance<nx_http::ClientPool>();
}

void QnStaticCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);

    NX_ASSERT(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an NX_ASSERT here? Something is wrong with resource data json file. */
}

QnStaticCommonModule::~QnStaticCommonModule()
{
    clear();
    nx::network::SocketGlobals::deinit();
}

Qn::PeerType QnStaticCommonModule::localPeerType() const
{
    return m_localPeerType;
}

QString QnStaticCommonModule::brand() const
{
    return m_brand;
}

QString QnStaticCommonModule::customization() const
{
    return m_customization;
}

QnSoftwareVersion QnStaticCommonModule::engineVersion() const
{
    return m_engineVersion;
}

void QnStaticCommonModule::setEngineVersion(const QnSoftwareVersion &version)
{
    m_engineVersion = version;
}

QnResourceDataPool * QnStaticCommonModule::dataPool() const
{
    return m_dataPool;
}

void QnStaticCommonModule::registerShortInstance(const QnUuid& id, int number)
{
    QnMutexLocker lock(&m_mutex);
    m_longToShortInstanceId.insert(id, number);
}

int QnStaticCommonModule::toShortInstance(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_longToShortInstanceId.find(id);
    return itr != m_longToShortInstanceId.end() ? itr.value() : -1;
}

QString QnStaticCommonModule::moduleDisplayName(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_longToShortInstanceId.find(id);
    return itr != m_longToShortInstanceId.end() ?
        QString::number(itr.value()) : id.toString();
}
