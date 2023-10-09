// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QCoreApplication>

#include <common/common_meta_types.h>
#include <core/resource/storage_plugin_factory.h>
#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <nx/branding.h>
#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/ffmpeg_initializer.h>

// Resources initialization must be located outside of the namespace.
static void initializeResources()
{
    Q_INIT_RESOURCE(nx_vms_common);
}

namespace nx::vms::common {

using namespace nx::network;

static ApplicationContext* s_instance = nullptr;

struct ApplicationContext::Private
{
    ApplicationContext* const q;

    cloud::tcp::EndpointVerificatorFactory::Function endpointVerificatorFactoryBak;
    mutable nx::Mutex mutex;
    QMap<QnUuid, int> longToShortInstanceId;
    const PeerType localPeerType;
    QString locale{nx::branding::defaultLocale()};

    std::unique_ptr<QnLongRunnablePool> longRunnablePool;
    std::unique_ptr<QnLongRunableCleanup> longRunableCleanup;
    std::unique_ptr<QnFfmpegInitializer> ffmpegInitializer;
    std::unique_ptr<QnSyncTime> syncTime;
    std::unique_ptr<QnStoragePluginFactory> storagePluginFactory;
    std::unique_ptr<nx::utils::TimerManager> timerManager;
};

ApplicationContext::ApplicationContext(
    PeerType localPeerType,
    const QString& customCloudHost,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .q = this,
        .localPeerType = localPeerType
    })
{
    if (NX_ASSERT(!s_instance))
        s_instance = this;

    initializeResources();
    QnCommonMetaTypes::initialize();

    d->longRunnablePool = std::make_unique<QnLongRunnablePool>();
    d->longRunableCleanup = std::make_unique<QnLongRunableCleanup>();
    d->ffmpegInitializer = std::make_unique<QnFfmpegInitializer>();
    QnFfmpegHelper::registerLogCallback();

    initNetworking(customCloudHost);

    d->syncTime = std::make_unique<QnSyncTime>();
    d->storagePluginFactory = std::make_unique<QnStoragePluginFactory>();
    d->timerManager = std::make_unique<nx::utils::TimerManager>("CommonTimerManager");
}

ApplicationContext::~ApplicationContext()
{
    d->longRunnablePool->stopAll();

    // Long runnables with async destruction should be destroyed before we deinitialize networking.
    d->longRunableCleanup.reset();
    d->longRunnablePool.reset();
    deinitNetworking();

    if (NX_ASSERT(s_instance == this))
        s_instance = nullptr;
}

ApplicationContext* ApplicationContext::instance()
{
    //NX_ASSERT(s_instance);
    return s_instance;
}

void ApplicationContext::initNetworking(const QString& customCloudHost)
{
    SocketGlobals::init(/*initializationFlags*/ 0, customCloudHost.toStdString());
    SocketGlobals::addressResolver().addFixedAddress("localhost", SocketAddress::anyPrivateAddress);

    // Providing mediaserver-specific way of validating peer id.
    d->endpointVerificatorFactoryBak =
        cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            [](const std::string& connectSessionId)
                -> std::unique_ptr<cloud::tcp::AbstractEndpointVerificator>
            {
                return std::make_unique<CloudMediaServerEndpointVerificator>(
                    connectSessionId);
            });
}

void ApplicationContext::deinitNetworking()
{
    cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
        std::move(d->endpointVerificatorFactoryBak));
    SocketGlobals::addressResolver().removeFixedAddress("localhost", SocketAddress::anyPrivateAddress);
    SocketGlobals::deinit();
}

nx::vms::api::PeerType ApplicationContext::localPeerType() const
{
    return d->localPeerType;
}

QString ApplicationContext::locale() const
{
    return d->locale;
}

void ApplicationContext::setLocale(const QString& value)
{
    d->locale = value;
}

void ApplicationContext::setModuleShortId(const QnUuid& id, int number)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->longToShortInstanceId.insert(id, number);
}

int ApplicationContext::moduleShortId(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ? itr.value() : -1;
}

QString ApplicationContext::moduleDisplayName(const QnUuid& id) const
{
    static const QnUuid kCloudPeerId("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB");
    if (id == kCloudPeerId)
        return "Cloud";

    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ?
        QString::number(itr.value()) : id.toString();
}


QnStoragePluginFactory* ApplicationContext::storagePluginFactory() const
{
    return d->storagePluginFactory.get();
}

} // namespace nx::vms::common
