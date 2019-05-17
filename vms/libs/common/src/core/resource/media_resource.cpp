#include "media_resource.h"

#include <QtGui/QImage>

#include <utils/common/warnings.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/math/fuzzy.h>

#include "camera_user_attribute_pool.h"
#include "resource_media_layout.h"
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/vms/api/types/motion_types.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/core/ptz/component.h>

namespace core_ptz = nx::core::ptz;

namespace {

static const QString customAspectRatioKey("overrideAr");
static const QString dontRecordPrimaryStreamKey("dontRecordPrimaryStream");
static const QString dontRecordSecondaryStreamKey("dontRecordSecondaryStream");
static const QString rtpTransportKey("rtpTransport");
static const QString dynamicVideoLayoutKey("dynamicVideoLayout");
static const QString rotationKey("rotation");
static const QString panicRecordingKey("panic_mode");

} // namespace

//-------------------------------------------------------------------------------------------------
// QnMediaResource
//-------------------------------------------------------------------------------------------------

QnMediaResource::QnMediaResource()
{
}

QnMediaResource::~QnMediaResource()
{
}

Qn::StreamQuality QnMediaResource::getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const
{
    return Qn::StreamQuality::normal;
}

QImage QnMediaResource::getImage(
    int /*channel*/, QDateTime /*time*/, Qn::StreamQuality /*quality*/) const
{
    return QImage();
}

static QSharedPointer<QnDefaultResourceVideoLayout> defaultVideoLayout(
    new QnDefaultResourceVideoLayout());
QnConstResourceVideoLayoutPtr QnMediaResource::getDefaultVideoLayout()
{
    return defaultVideoLayout;
}

QnConstResourceVideoLayoutPtr QnMediaResource::getVideoLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    QnMutexLocker lock(&m_layoutMutex);

#ifdef ENABLE_DATA_PROVIDERS
    if (dataProvider)
    {
        QnConstResourceVideoLayoutPtr providerLayout = dataProvider->getVideoLayout();
        if (providerLayout)
            return providerLayout;
    }
#endif //ENABLE_DATA_PROVIDERS

    QString strVal = toResource()->getProperty(ResourcePropertyKey::kVideoLayout);
    if (strVal.isEmpty())
    {
        return defaultVideoLayout;
    }
    else
    {
        if (m_cachedLayout != strVal || !m_customVideoLayout)
        {
            m_customVideoLayout = QnCustomResourceVideoLayout::fromString(strVal);
            m_cachedLayout = strVal;
        }
        return m_customVideoLayout;
    }
}

static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout(new QnEmptyResourceAudioLayout());
QnConstResourceAudioLayoutPtr QnMediaResource::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return audioLayout;
}

void QnMediaResource::initMediaResource()
{
    toResource()->addFlags(Qn::media);
}

QnCameraUserAttributePool* QnMediaResource::userAttributesPool() const
{
    return toResource()->commonModule()->cameraUserAttributesPool();
}

QnMediaDewarpingParams QnMediaResource::getDewarpingParams() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock(
        userAttributesPool(), toResource()->getId());
    return (*userAttributesLock)->dewarpingParams;
}

void QnMediaResource::setDewarpingParams(const QnMediaDewarpingParams& params)
{
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            userAttributesPool(), toResource()->getId());

        if ((*userAttributesLock)->dewarpingParams == params)
            return;

        (*userAttributesLock)->dewarpingParams = params;
    }
    emit toResource()->mediaDewarpingParamsChanged(this->toResourcePtr());
}

QnAspectRatio QnMediaResource::customAspectRatio() const
{
    if (!this->toResource()->hasProperty(::customAspectRatioKey))
        return QnAspectRatio();

    bool ok = true;
    qreal value = this->toResource()->getProperty(::customAspectRatioKey).toDouble(&ok);
    if (!ok || qIsNaN(value) || qIsInf(value) || value < 0)
        return QnAspectRatio();

    return QnAspectRatio::closestStandardRatio(value);
}

void QnMediaResource::setCustomAspectRatio(const QnAspectRatio& value)
{
    if (!value.isValid())
        clearCustomAspectRatio();
    else
        this->toResource()->setProperty(::customAspectRatioKey, QString::number(value.toFloat()));
}

void QnMediaResource::clearCustomAspectRatio()
{
    this->toResource()->setProperty(::customAspectRatioKey, QString());
}

qreal QnMediaResource::defaultRotation() const
{
    return toResource()->getProperty(rotationKey()).toInt();
}

Ptz::Capabilities QnMediaResource::getPtzCapabilities(core_ptz::Type ptzType) const
{
    switch (ptzType)
    {
        case core_ptz::Type::operational:
            return Ptz::Capabilities(toResource()->getProperty(
                ResourcePropertyKey::kPtzCapabilities).toInt());

        case core_ptz::Type::configurational:
            return Ptz::Capabilities(toResource()->getProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities).toInt());
        default:
            NX_ASSERT(false, "Wrong ptz type, we should never be here");
            return Ptz::NoPtzCapabilities;
    }
}

bool QnMediaResource::hasAnyOfPtzCapabilities(
    Ptz::Capabilities capabilities,
    nx::core::ptz::Type ptzType) const
{
    return getPtzCapabilities(ptzType) & capabilities;
}

void QnMediaResource::setPtzCapabilities(
    Ptz::Capabilities capabilities,
    core_ptz::Type ptzType)
{
    switch (ptzType)
    {
        case core_ptz::Type::operational:
        {
            toResource()->setProperty(
                ResourcePropertyKey::kPtzCapabilities, (int) capabilities);
            break;
        }
        case core_ptz::Type::configurational:
        {
            toResource()->setProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities, (int) capabilities);
            break;
        }
        default:
            NX_ASSERT(false, "Wrong ptz type, we should never be here");
    }

}

void QnMediaResource::setPtzCapability(
    Ptz::Capabilities capability,
    bool value,
    core_ptz::Type ptzType)
{
    setPtzCapabilities(value
        ? (getPtzCapabilities(ptzType) | capability)
        : (getPtzCapabilities(ptzType) & ~capability),
        ptzType);
}

bool QnMediaResource::canSwitchPtzPresetTypes() const
{
    const auto capabilities = getPtzCapabilities();
    if (!(capabilities & Ptz::NativePresetsPtzCapability))
        return false;

    if (capabilities & Ptz::NoNxPresetsPtzCapability)
        return false;

    // Check if our server can emulate presets.
    return (capabilities & Ptz::AbsolutePtrzCapabilities)
        && (capabilities & Ptz::PositioningPtzCapabilities);
}

QString QnMediaResource::customAspectRatioKey()
{
    return ::customAspectRatioKey;
}

QString QnMediaResource::dontRecordPrimaryStreamKey()
{
    return ::dontRecordPrimaryStreamKey;
}

QString QnMediaResource::dontRecordSecondaryStreamKey()
{
    return ::dontRecordSecondaryStreamKey;
}

QString QnMediaResource::rtpTransportKey()
{
    return ::rtpTransportKey;
}

QString QnMediaResource::panicRecordingKey()
{
    return ::panicRecordingKey;
}

QString QnMediaResource::dynamicVideoLayoutKey()
{
    return ::dynamicVideoLayoutKey;
}

QString QnMediaResource::rotationKey()
{
    return ::rotationKey;
}
