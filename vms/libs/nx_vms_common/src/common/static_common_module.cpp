// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "static_common_module.h"

#include <QtCore/QCoreApplication>

#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/socket_global.h>
#include <network/cloud/cloud_media_server_endpoint_verificator.h>

#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_initializer.h>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>

#include "common_meta_types.h"

using namespace nx::network;

struct QnStaticCommonModule::Private
{
    Private(nx::vms::api::PeerType localPeerType):
        localPeerType(localPeerType)
    {
    }

    cloud::tcp::EndpointVerificatorFactory::Function endpointVerificatorFactoryBak;
    mutable nx::Mutex mutex;
    QMap<QnUuid, int> longToShortInstanceId;
    const PeerType localPeerType;

    std::unique_ptr<QnLongRunnablePool> longRunnablePool;
    std::unique_ptr<QnLongRunableCleanup> longRunableCleanup;
    std::unique_ptr<QnFfmpegInitializer> ffmpegInitializer;
    std::unique_ptr<QnSyncTime> syncTime;
};

template<> QnStaticCommonModule* Singleton<QnStaticCommonModule>::s_instance = nullptr;

QnStaticCommonModule::QnStaticCommonModule(
    PeerType localPeerType,
    const QString& customCloudHost)
    :
    d(new Private(localPeerType))
{
    Q_INIT_RESOURCE(nx_vms_common);
    QnCommonMetaTypes::initialize();

    d->longRunnablePool = std::make_unique<QnLongRunnablePool>();
    d->longRunableCleanup = std::make_unique<QnLongRunableCleanup>();
    d->ffmpegInitializer = std::make_unique<QnFfmpegInitializer>();

    initNetworking(customCloudHost);

    d->syncTime = std::make_unique<QnSyncTime>();
}

QnStaticCommonModule::~QnStaticCommonModule()
{
    d->longRunnablePool->stopAll();
    deinitNetworking();
}

void QnStaticCommonModule::initNetworking(const QString& customCloudHost)
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

void QnStaticCommonModule::deinitNetworking()
{
    cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
        std::move(d->endpointVerificatorFactoryBak));
    SocketGlobals::addressResolver().removeFixedAddress("localhost", SocketAddress::anyPrivateAddress);
    SocketGlobals::deinit();
}

nx::vms::api::PeerType QnStaticCommonModule::localPeerType() const
{
    return d->localPeerType;
}

void QnStaticCommonModule::setModuleShortId(const QnUuid& id, int number)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->longToShortInstanceId.insert(id, number);
}

int QnStaticCommonModule::moduleShortId(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ? itr.value() : -1;
}

QString QnStaticCommonModule::moduleDisplayName(const QnUuid& id) const
{
    static const QnUuid kCloudPeerId("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB");
    if (id == kCloudPeerId)
        return "Cloud";

    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ?
        QString::number(itr.value()) : id.toString();
}
