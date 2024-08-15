// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_camera.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx_ec/abstract_ec_connection.h>

namespace {

const QString kAutoSendPtzStopCommandProperty("autoSendPtzStopCommand");

} // namespace

QnClientCameraResource::QnClientCameraResource(const nx::Uuid& resourceTypeId):
    base_type(resourceTypeId)
{
    // Handle situation when flags are added externally after resource is created.
    connect(this, &QnResource::flagsChanged, this,
        [this]() { m_cachedFlags.store(calculateFlags()); }, Qt::DirectConnection);
}

Qn::ResourceFlags QnClientCameraResource::calculateFlags() const
{
    Qn::ResourceFlags result = base_type::flags();
    if (!isDtsBased() && (supportedMotionTypes() != MotionTypes{MotionType::none}))
        result |= Qn::motion;
    return result;
}

Qn::ResourceFlags QnClientCameraResource::flags() const
{
    if (m_cachedFlags.load() == Qn::ResourceFlags())
        m_cachedFlags.store(calculateFlags());
    return m_cachedFlags;
}

void QnClientCameraResource::resetCachedValues()
{
    base_type::resetCachedValues();
    m_cachedFlags = calculateFlags();
}

void QnClientCameraResource::setAuthToCameraGroup(
    const QnVirtualCameraResourcePtr& camera,
    const QAuthenticator& authenticator)
{
    NX_ASSERT(camera->isMultiSensorCamera() || camera->isNvr());
    if (!camera->resourcePool())
        return;

    const auto groupId = camera->getGroupId();
    NX_ASSERT(!groupId.isEmpty());
    if (groupId.isEmpty())
        return;

    auto sensors = camera->resourcePool()->getResources<QnVirtualCameraResource>(
        [groupId](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->getGroupId() == groupId;
        });

    for (const auto& sensor: sensors)
    {
        sensor->setAuth(authenticator);
        sensor->savePropertiesAsync();
    }
}

QnAbstractStreamDataProvider* QnClientCameraResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    const auto camera = resource.dynamicCast<QnClientCameraResource>();
    NX_ASSERT(camera && role == Qn::CR_Default);
    if (!camera)
        return nullptr;

     QnAbstractStreamDataProvider* result = camera->createLiveDataProvider();
     if (result)
         result->setRole(role);
     return result;
}

QString QnClientCameraResource::idForToStringFromPtr() const
{
    return QnResource::idForToStringFromPtr();
}

bool QnClientCameraResource::autoSendPtzStopCommand() const
{
    return getProperty(kAutoSendPtzStopCommandProperty).isEmpty();
}

void QnClientCameraResource::setAutoSendPtzStopCommand(bool value)
{
    setProperty(kAutoSendPtzStopCommandProperty, value ? QString() : QString("0"));
}

QnConstResourceVideoLayoutPtr QnClientCameraResource::getVideoLayout(
    const QnAbstractStreamDataProvider* dataProvider)
{
    return QnVirtualCameraResource::getVideoLayout(dataProvider);
}

AudioLayoutConstPtr QnClientCameraResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    if (const auto archive = dynamic_cast<const QnArchiveStreamReader*>(dataProvider))
        return archive->getDPAudioLayout();

    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider* QnClientCameraResource::createLiveDataProvider()
{
    const auto camera = toSharedPointer(this);
    auto context = dynamic_cast<nx::vms::client::core::SystemContext*>(systemContext());
    if (!NX_ASSERT(context))
        return nullptr;

    auto credentials = context->connection()
        ? context->connection()->credentials()
        : nx::network::http::Credentials();

    QnArchiveStreamReader *result = new QnArchiveStreamReader(camera);
    auto delegate = new QnRtspClientArchiveDelegate(
        result,
        credentials,
        "ClientCamera");

    delegate->setCamera(camera);
    result->setArchiveDelegate(delegate);

    connect(delegate, &QnRtspClientArchiveDelegate::dataDropped, this,
        &QnClientCameraResource::dataDropped);

    connect(delegate, &QnRtspClientArchiveDelegate::needUpdateTimeLine, this,
        &QnClientCameraResource::footageAdded);

    if (auto connection = context->connection())
    {
        connect(connection.get(),
            &nx::vms::client::core::RemoteConnection::credentialsChanged,
            delegate,
            [this, delegate,
                connectionPtr = std::weak_ptr<nx::vms::client::core::RemoteConnection>(connection)]
            {
                if (auto connection = connectionPtr.lock())
                    delegate->updateCredentials(connection->credentials());
            });
    }

    return result;
}
