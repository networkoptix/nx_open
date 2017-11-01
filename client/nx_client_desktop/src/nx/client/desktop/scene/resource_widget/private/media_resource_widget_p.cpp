#include "media_resource_widget_p.h"

#include <core/resource/camera_resource.h>

#include <camera/cam_display.h>
#include <camera/resource_display.h>

#include <utils/license_usage_helper.h>

namespace nx {
namespace client {
namespace desktop {

MediaResourceWidgetPrivate::MediaResourceWidgetPrivate(const QnResourcePtr& resource,
    QObject* parent)
    :
    base_type(parent),
    resource(resource),
    mediaResource(resource.dynamicCast<QnMediaResource>()),
    camera(resource.dynamicCast<QnVirtualCameraResource>()),
    hasVideo(mediaResource->hasVideo(nullptr)),
    isIoModule(camera && camera->hasFlags(Qn::io_module))
{
    QSignalBlocker blocker(this);

    NX_ASSERT(mediaResource);

    connect(resource, &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsOffline);
    connect(resource, &QnResource::statusChanged, this,
        &MediaResourceWidgetPrivate::updateIsUnauthorized);

    if (camera)
    {
        m_licenseStatusHelper.reset(new QnSingleCamLicenseStatusHelper(camera));
        connect(m_licenseStatusHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged,
            this, &MediaResourceWidgetPrivate::updateNvrWithoutLicense);

        updateNvrWithoutLicense();
    }
}

QnResourceDisplayPtr MediaResourceWidgetPrivate::display() const
{
    return m_display;
}

void MediaResourceWidgetPrivate::setDisplay(const QnResourceDisplayPtr& display)
{
    NX_ASSERT(display && !m_display);
    m_display = display;
    connect(m_display->camDisplay(), &QnCamDisplay::liveMode, this,
        &MediaResourceWidgetPrivate::updateIsPlayingLive);

    {
        QSignalBlocker blocker(this);
        updateIsPlayingLive();
        updateIsOffline();
        updateIsUnauthorized();
        updateNvrWithoutLicense();
    }
}

bool MediaResourceWidgetPrivate::isPlayingLive() const
{
    return m_isPlayingLive;
}

bool MediaResourceWidgetPrivate::isOffline() const
{
    return m_isOffline;
}

bool MediaResourceWidgetPrivate::isUnauthorized() const
{
    return m_isUnauthorized;
}

bool MediaResourceWidgetPrivate::nvrWithoutLicense() const
{
    return m_nvrWithoutLicense;
}

void MediaResourceWidgetPrivate::updateIsPlayingLive()
{
    setIsPlayingLive(m_display && m_display->camDisplay()->isRealTimeSource());
}

void MediaResourceWidgetPrivate::setIsPlayingLive(bool value)
{
    if (m_isPlayingLive == value)
        return;

    m_isPlayingLive = value;

    {
        QSignalBlocker blocker(this);
        updateIsOffline();
        updateIsUnauthorized();
        updateNvrWithoutLicense();
    }

    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateIsOffline()
{
    setIsOffline(m_isPlayingLive && (resource->getStatus() == Qn::Offline));
}

void MediaResourceWidgetPrivate::setIsOffline(bool value)
{
    if (m_isOffline == value)
        return;

    m_isOffline = value;
    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateIsUnauthorized()
{
    setIsUnauthorized(m_isPlayingLive && (resource->getStatus() == Qn::Unauthorized));
}

void MediaResourceWidgetPrivate::setIsUnauthorized(bool value)
{
    if (m_isUnauthorized == value)
        return;

    m_isUnauthorized = value;
    emit stateChanged();
}

void MediaResourceWidgetPrivate::updateNvrWithoutLicense()
{
    // Footage browsing is forbidden for NVRs without license. Live video is forbidden for VMAX.
    setNvrWithoutLicense(camera
        && camera->isDtsBased()
        && m_licenseStatusHelper->status() != QnSingleCamLicenseStatusHelper::LicenseStatus::used
        && (!m_isPlayingLive || camera->licenseType() != Qn::LC_Bridge));
}

void MediaResourceWidgetPrivate::setNvrWithoutLicense(bool value)
{
    if (m_nvrWithoutLicense == value)
        return;

    m_nvrWithoutLicense = value;
    emit stateChanged();
}


} // namespace desktop
} // namespace client
} // namespace nx
