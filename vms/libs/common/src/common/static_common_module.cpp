#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/socket_global.h>

#include <api/http_client_pool.h>
#include <core/resource_management/resource_data_pool.h>
#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <utils/common/app_info.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_initializer.h>

#include "common_meta_types.h"

using namespace nx;

struct QnStaticCommonModulePrivate
{
    nx::network::cloud::tcp::EndpointVerificatorFactory::Function endpointVerificatorFactoryBak;
};

QnStaticCommonModule::QnStaticCommonModule(
    vms::api::PeerType localPeerType,
    const QString& brand,
    const QString& customization,
    const QString& customCloudHost,
    QObject *parent)
    :
    QObject(parent),
    m_private(new QnStaticCommonModulePrivate),
    m_localPeerType(localPeerType),
    m_brand(brand),
    m_customization(customization)
{
    Q_INIT_RESOURCE(common);
    QnCommonMetaTypes::initialize();
    instance<QnLongRunnablePool>();
    instance<QnFfmpegInitializer>();
    nx::network::SocketGlobals::init(/*initializationFlags*/ 0, customCloudHost);

    // Providing mediaserver-specific way of validating peer id.
    m_private->endpointVerificatorFactoryBak =
        nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            [](const std::string& connectSessionId)
                -> std::unique_ptr<nx::network::cloud::tcp::AbstractEndpointVerificator>
            {
                return std::make_unique<CloudMediaServerEndpointVerificator>(
                    connectSessionId);
            });

    store(new QnLongRunableCleanup());
    store(new nx::utils::TimerManager("QnStaticCommonModule::StandaloneTimerManager"));

    instance<QnSyncTime>();
    instance<nx::network::http::ClientPool>();
}

QnStaticCommonModule::~QnStaticCommonModule()
{
    instance<QnLongRunnablePool>()->stopAll();
    nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
        std::move(m_private->endpointVerificatorFactoryBak));
    nx::network::SocketGlobals::deinit();

    clear();

    delete m_private;
    m_private = nullptr;
}

vms::api::PeerType QnStaticCommonModule::localPeerType() const
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
