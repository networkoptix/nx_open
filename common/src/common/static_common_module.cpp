#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/socket_global.h>

#include <api/http_client_pool.h>
#include <core/resource_management/resource_data_pool.h>
#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>

#include "common_meta_types.h"

struct QnStaticCommonModulePrivate
{
    nx::network::cloud::tcp::EndpointVerificatorFactory::Function endpointVerificatorFactoryBak;
};

QnStaticCommonModule::QnStaticCommonModule(
    Qn::PeerType localPeerType,
    const QString& brand,
    const QString& customization,
    QObject *parent)
    :
    QObject(parent),
    m_private(new QnStaticCommonModulePrivate),
    m_localPeerType(localPeerType),
    m_brand(brand),
    m_customization(customization),
    m_engineVersion(QnAppInfo::engineVersion())
{
    Q_INIT_RESOURCE(common);
    QnCommonMetaTypes::initialize();
    instance<QnLongRunnablePool>();
    nx::network::SocketGlobals::init();

    // Providing mediaserver-specific way of validating peer id.
    m_private->endpointVerificatorFactoryBak =
        nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            [](const nx::String& connectSessionId) 
                -> std::unique_ptr<nx::network::cloud::tcp::AbstractEndpointVerificator>
            {
                return std::make_unique<CloudMediaServerEndpointVerificator>(
                    connectSessionId);
            });

    m_dataPool = instance<QnResourceDataPool>();
    loadResourceData(m_dataPool, lit(":/resource_data.json"), true);
    loadResourceData(m_dataPool, QCoreApplication::applicationDirPath() + lit("/resource_data.json"), false);

    store(new QnLongRunableCleanup());
    store(new nx::utils::TimerManager());

    instance<QnSyncTime>();
    instance<nx::network::http::ClientPool>();
}

void QnStaticCommonModule::loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required) {
    bool loaded = QFile::exists(fileName) && dataPool->load(fileName);

    NX_ASSERT(!required || loaded, Q_FUNC_INFO, "Can't parse resource_data.json file!");  /* Getting an NX_ASSERT here? Something is wrong with resource data json file. */
}

QnStaticCommonModule::~QnStaticCommonModule()
{
    clear();

    nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
        std::move(m_private->endpointVerificatorFactoryBak));
    nx::network::SocketGlobals::deinit();

    delete m_private;
    m_private = nullptr;
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

void QnStaticCommonModule::setModuleShortId(const QnUuid& id, int number)
{
    QnMutexLocker lock(&m_mutex);
    m_longToShortInstanceId.insert(id, number);
}

int QnStaticCommonModule::moduleShortId(const QnUuid& id) const
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
