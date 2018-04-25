#include "media_resource.h"

#include <QtGui/QImage>


#include <utils/common/warnings.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/math/fuzzy.h>

#include "camera_user_attribute_pool.h"
#include "resource_media_layout.h"
#include "nx/streaming/abstract_stream_data_provider.h"
#include <common/common_module.h>

namespace {
    const QString customAspectRatioKey          = lit("overrideAr");
    const QString dontRecordPrimaryStreamKey    = lit("dontRecordPrimaryStream");
    const QString dontRecordSecondaryStreamKey  = lit("dontRecordSecondaryStream");
    const QString rtpTransportKey               = lit("rtpTransport");
    const QString dynamicVideoLayoutKey         = lit("dynamicVideoLayout");
    const QString motionStreamKey               = lit("motionStream");
    const QString rotationKey                   = lit("rotation");
    const QString panicRecordingKey             = lit("panic_mode");

    const QString primaryStreamValue            = lit("primary");
    const QString secondaryStreamValue          = lit("secondary");
    const QString edgeStreamValue               = lit("edge");

    /** Special value for absent custom aspect ratio. Should not be changed without a reason because a lot of modules check it as qFuzzyIsNull. */
    const qreal noCustomAspectRatio = 0.0;
}

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
    if (!m_hasVideo.is_initialized())
        m_hasVideo = toResource()->getProperty(Qn::VIDEO_DISABLED_PARAM_NAME).toInt() == 0;
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

Ptz::Capabilities QnMediaResource::getPtzCapabilities() const
{
    return Ptz::Capabilities(toResource()->getProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME).toInt());
}

bool QnMediaResource::hasAnyOfPtzCapabilities(Ptz::Capabilities capabilities) const
{
    return getPtzCapabilities() & capabilities;
}

void QnMediaResource::setPtzCapabilities(Ptz::Capabilities capabilities)
{
    if (toResource()->hasParam(Qn::PTZ_CAPABILITIES_PARAM_NAME))
        toResource()->setProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
}

void QnMediaResource::setPtzCapability(Ptz::Capabilities capability, bool value)
{
    setPtzCapabilities(value
        ? (getPtzCapabilities() | capability)
        : (getPtzCapabilities() & ~capability));
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
