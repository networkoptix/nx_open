// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/status_dictionary.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

using namespace nx::vms::api;

namespace {

const QString kAutoSendPtzStopCommandProperty("autoSendPtzStopCommand");

} // namespace

Camera::Camera(const nx::Uuid& resourceTypeId)
{
    setTypeId(resourceTypeId);
    addFlags(Qn::server_live_cam);

    // Handle situation when flags are added externally after resource is created.
    connect(this, &QnResource::flagsChanged, this,
        [this]() { m_cachedFlags.store(calculateFlags()); }, Qt::DirectConnection);
}

QString Camera::getName() const
{
    return getUserDefinedName();
}

void Camera::setName(const QString& name)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.cameraName == name)
            return;
        m_userAttributes.cameraName = name;
    }
    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags Camera::calculateFlags() const
{
    Qn::ResourceFlags result = base_type::flags();
    if (isIOModule())
        result |= Qn::io_module;

    if (!isDtsBased() && (supportedMotionTypes() != MotionTypes{MotionType::none}))
        result |= Qn::motion;

    return result;
}

Qn::ResourceFlags Camera::flags() const
{
    if (m_cachedFlags.load() == Qn::ResourceFlags())
        m_cachedFlags.store(calculateFlags());

    return m_cachedFlags;
}

void Camera::resetCachedValues()
{
    base_type::resetCachedValues();
    m_cachedFlags.store(calculateFlags());
}

ResourceStatus Camera::getStatus() const
{
    if (auto context = systemContext())
    {
        const auto serverStatus = context->resourceStatusDictionary()->value(getParentId());
        if (serverStatus == ResourceStatus::offline
            || serverStatus == ResourceStatus::unauthorized
            || serverStatus == ResourceStatus::mismatchedCertificate)
        {
            return ResourceStatus::offline;
        }
    }
    return QnResource::getStatus();
}

void Camera::setParentId(const nx::Uuid& parent)
{
    nx::Uuid oldValue = getParentId();
    if (oldValue != parent)
    {
        base_type::setParentId(parent);
        if (!oldValue.isNull())
            emit statusChanged(toSharedPointer(this), Qn::StatusChangeReason::Local);
    }
}

bool Camera::hasAudio() const
{
    auto context = qobject_cast<SystemContext*>(systemContext());
    if (!NX_ASSERT(context))
        return false;

    // If we don't have PlayAudioPermission for this camera then it effectively doesn't have audio.
    return base_type::hasAudio()
        && context->accessController()->hasPermissions(toSharedPointer(), Qn::PlayAudioPermission);
}

QnConstResourceVideoLayoutPtr Camera::getVideoLayout(
    const QnAbstractStreamDataProvider* dataProvider)
{
    return QnVirtualCameraResource::getVideoLayout(dataProvider);
}

AudioLayoutConstPtr Camera::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    if (const auto archive = dynamic_cast<const QnArchiveStreamReader*>(dataProvider))
        return archive->getDPAudioLayout();

    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider* Camera::createDataProvider(Qn::ConnectionRole role)
{
    NX_ASSERT(role == Qn::CR_Default);
    const bool needRtsp = hasVideo() || hasAudio();
    if (!needRtsp)
        return {};

    const auto context = dynamic_cast<nx::vms::client::core::SystemContext*>(systemContext());
    if (!NX_ASSERT(context))
        return nullptr;

    const auto credentials = context->connection()
        ? context->connection()->credentials()
        : nx::network::http::Credentials();

    const auto camera = toSharedPointer(this);
    const auto result = new QnArchiveStreamReader(camera);
    const auto delegate = new QnRtspClientArchiveDelegate(result, credentials, "ClientCamera");

    delegate->setCamera(camera);
    result->setArchiveDelegate(delegate);

    connect(delegate, &QnRtspClientArchiveDelegate::dataDropped,
        this, &Camera::dataDropped);

    connect(delegate, &QnRtspClientArchiveDelegate::needUpdateTimeLine,
        this, &Camera::footageAdded);

    if (auto connection = context->connection())
    {
        connect(connection.get(), &RemoteConnection::credentialsChanged, delegate,
            [delegate, connectionPtr = std::weak_ptr<RemoteConnection>(connection)]
            {
                if (auto connection = connectionPtr.lock())
                    delegate->updateCredentials(connection->credentials());
            });
    }

    result->setRole(role);
    return result;
}

bool Camera::isPtzSupported() const
{
    // Camera must have at least one ptz control capability but fisheye must be disabled.
    return hasAnyOfPtzCapabilities(
            Ptz::Capability::continuousPanTiltZoom | Ptz::Capability::viewport)
        && !hasAnyOfPtzCapabilities(Ptz::Capability::virtual_);
}

bool Camera::isPtzRedirected() const
{
    return !getProperty(nx::vms::api::device_properties::kPtzTargetId).isEmpty();
}

CameraPtr Camera::ptzRedirectedTo() const
{
    const auto redirectId = getProperty(nx::vms::api::device_properties::kPtzTargetId);
    if (redirectId.isEmpty() || !resourcePool())
        return {};

    return resourcePool()->getResourceByPhysicalId<Camera>(redirectId);
}

bool Camera::autoSendPtzStopCommand() const
{
    return getProperty(kAutoSendPtzStopCommandProperty).isEmpty();
}

void Camera::setAutoSendPtzStopCommand(bool value)
{
    setProperty(kAutoSendPtzStopCommandProperty, value ? QString() : QString("0"));
}

void Camera::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    if (source->getParentId() != m_parentId)
    {
        notifiers <<
            [r = toSharedPointer(this)]
            {
                emit r->statusChanged(r, Qn::StatusChangeReason::Local);
            };
    }
    base_type::updateInternal(source, notifiers);
}

QString Camera::idForToStringFromPtr() const
{
    return QnResource::idForToStringFromPtr();
}

void Camera::setAuthToCameraGroup(
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

} // namespace nx::vms::client::core
