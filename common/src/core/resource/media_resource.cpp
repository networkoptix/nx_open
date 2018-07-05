#include "media_resource.h"

#include <QtGui/QImage>


#include <utils/common/warnings.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/math/fuzzy.h>

#include "camera_user_attribute_pool.h"
#include "resource_media_layout.h"
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/vms/api/types/motion_types.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/core/ptz/component.h>

namespace core_ptz = nx::core::ptz;

namespace {

static const QString customAspectRatioKey = lit("overrideAr");
static const QString dontRecordPrimaryStreamKey = lit("dontRecordPrimaryStream");
static const QString dontRecordSecondaryStreamKey = lit("dontRecordSecondaryStream");
static const QString rtpTransportKey = lit("rtpTransport");
static const QString dynamicVideoLayoutKey = lit("dynamicVideoLayout");
static const QString motionStreamKey = lit("motionStream");
static const QString rotationKey = lit("rotation");
static const QString panicRecordingKey = lit("panic_mode");

using nx::vms::api::MotionStreamType;
static const QString primaryStreamValue = QnLexical::serialized(MotionStreamType::primary);
static const QString secondaryStreamValue = QnLexical::serialized(MotionStreamType::secondary);
static const QString edgeStreamValue = QnLexical::serialized(MotionStreamType::edge);

/** Special value for absent custom aspect ratio. Should not be changed without a reason because a lot of modules check it as qFuzzyIsNull. */
static constexpr qreal noCustomAspectRatio = 0.0;

} // namespace

// -------------------------------------------------------------------------- //
// QnMediaResource
// -------------------------------------------------------------------------- //
QnMediaResource::QnMediaResource()
{
}

QnMediaResource::~QnMediaResource()
{
}

Qn::StreamQuality QnMediaResource::getBestQualityForSuchOnScreenSize(const QSize&) const
{
    return Qn::StreamQuality::normal;
}

QImage QnMediaResource::getImage(int /*channel*/, QDateTime /*time*/, Qn::StreamQuality /*quality*/) const
{
    return QImage();
}

static QSharedPointer<QnDefaultResourceVideoLayout> defaultVideoLayout( new QnDefaultResourceVideoLayout() );

QnConstResourceVideoLayoutPtr QnMediaResource::getDefaultVideoLayout()
{
    return defaultVideoLayout;
}

QnConstResourceVideoLayoutPtr QnMediaResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    QnMutexLocker lock( &m_layoutMutex );

#ifdef ENABLE_DATA_PROVIDERS
    if (dataProvider) {
        QnConstResourceVideoLayoutPtr providerLayout = dataProvider->getVideoLayout();
        if (providerLayout)
            return providerLayout;
    }
#endif //ENABLE_DATA_PROVIDERS

    QString strVal = toResource()->getProperty(Qn::VIDEO_LAYOUT_PARAM_NAME);
    if (strVal.isEmpty())
    {
        return defaultVideoLayout;
    }
    else {
        if (m_cachedLayout != strVal || !m_customVideoLayout) {
            m_customVideoLayout = QnCustomResourceVideoLayout::fromString(strVal);
            m_cachedLayout = strVal;
        }
        return m_customVideoLayout;
    }
}

static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr QnMediaResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return audioLayout;
}

bool QnMediaResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    const auto cameraResource = toResourcePtr().dynamicCast<QnSecurityCamResource>();
    if (!cameraResource)
        return false;

    if (!m_hasVideo.is_initialized())
    {
        const auto data = qnStaticCommon->dataPool()->data(cameraResource);
        m_hasVideo = !data.value(Qn::VIDEO_DISABLED_PARAM_NAME, false);
    }
    return *m_hasVideo;
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
    QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), toResource()->getId() );
    return (*userAttributesLock)->dewarpingParams;
}

void QnMediaResource::setDewarpingParams(const QnMediaDewarpingParams& params)
{
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), toResource()->getId() );
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

Ptz::Capabilities QnMediaResource::getPtzCapabilities(core_ptz::Type ptzType) const
{
    switch (ptzType)
    {
        case core_ptz::Type::operational:
            return operationalPtzCapabilities();
        case core_ptz::Type::configurational:
            return Ptz::Capabilities(
                toResource()->getProperty(Qn::kConfigurationalPtzCapabilities).toInt());
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
            if (toResource()->hasParam(Qn::PTZ_CAPABILITIES_PARAM_NAME)) //< Why do we need this check?
                toResource()->setProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
        }
        case core_ptz::Type::configurational:
        {
            toResource()->setProperty(Qn::kConfigurationalPtzCapabilities, static_cast<int>(capabilities));
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
        : (getPtzCapabilities(ptzType) & ~capability), ptzType);
}

Ptz::Capabilities QnMediaResource::operationalPtzCapabilities() const
{
    return Ptz::Capabilities(toResource()->getProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME).toInt());
}

void QnMediaResource::setOperationalPtzCapabilities(Ptz::Capabilities capabilities)
{
    if (toResource()->hasParam(Qn::PTZ_CAPABILITIES_PARAM_NAME))
        toResource()->setProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
}

bool QnMediaResource::canDisableNativePtzPresets() const
{
    const auto caps = getPtzCapabilities();
    return caps.testFlag(Ptz::NativePresetsPtzCapability)
        && !caps.testFlag(Ptz::NoNxPresetsPtzCapability);
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

QString QnMediaResource::motionStreamKey()
{
    return ::motionStreamKey;
}

QString QnMediaResource::primaryStreamValue()
{
    return ::primaryStreamValue;
}

QString QnMediaResource::secondaryStreamValue()
{
    return ::secondaryStreamValue;
}

QString QnMediaResource::edgeStreamValue()
{
    return ::edgeStreamValue;
}

QString QnMediaResource::rotationKey()
{
    return ::rotationKey;
}
