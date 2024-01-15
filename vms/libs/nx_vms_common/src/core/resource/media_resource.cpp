// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_resource.h"

#include <QtGui/QImage>

#include <core/resource/resource_media_layout.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/api/types/motion_types.h>

#include "resource_media_layout.h"

using namespace nx::vms::common;

namespace {

static const QString customAspectRatioKey("overrideAr");
static const QString rtpTransportKey("rtpTransport");
static const QString dynamicVideoLayoutKey("dynamicVideoLayout");
static const QString panicRecordingKey("panic_mode");

} // namespace

//-------------------------------------------------------------------------------------------------
// QnMediaResource
//-------------------------------------------------------------------------------------------------

const QString QnMediaResource::kRotationKey("rotation");

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

static QnConstResourceVideoLayoutPtr defaultVideoLayout(new QnDefaultResourceVideoLayout());
QnConstResourceVideoLayoutPtr QnMediaResource::getDefaultVideoLayout()
{
    return defaultVideoLayout;
}

QnConstResourceVideoLayoutPtr QnMediaResource::getVideoLayout(
    const QnAbstractStreamDataProvider* dataProvider)
{
    NX_MUTEX_LOCKER lock(&m_layoutMutex);

    if (dataProvider)
    {
        QnConstResourceVideoLayoutPtr providerLayout = dataProvider->getVideoLayout();
        if (providerLayout)
            return providerLayout;
    }

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

static AudioLayoutConstPtr audioLayout(new AudioLayout());
AudioLayoutConstPtr QnMediaResource::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return audioLayout;
}

void QnMediaResource::initMediaResource()
{
    toResource()->addFlags(Qn::media);
}

nx::vms::api::dewarping::MediaData QnMediaResource::getDewarpingParams() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    if (!m_cachedDewarpingParams)
    {
        m_cachedDewarpingParams = QJson::deserialized<nx::vms::api::dewarping::MediaData>(
            m_userAttributes.dewarpingParams);
    }
    return *m_cachedDewarpingParams;
}

void QnMediaResource::setDewarpingParams(const nx::vms::api::dewarping::MediaData& params)
{
    {
        auto dewarpData = QJson::serialized(params);
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.dewarpingParams == dewarpData)
            return;

        m_cachedDewarpingParams.reset();
        m_userAttributes.dewarpingParams = dewarpData;
    }
    emit toResource()->mediaDewarpingParamsChanged(this->toResourcePtr());
}

QnAspectRatio QnMediaResource::customAspectRatio() const
{
    const QString propertyValue = this->toResource()->getProperty(::customAspectRatioKey);
    if (propertyValue.isEmpty())
        return QnAspectRatio();

    bool ok = true;
    const qreal value = propertyValue.toDouble(&ok);
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

std::optional<int> QnMediaResource::forcedRotation() const
{
    const auto rotation = toResource()->getProperty(kRotationKey);
    if (rotation.isEmpty())
        return std::nullopt;

    return rotation.toInt();
}

void QnMediaResource::setForcedRotation(std::optional<int> value)
{
    QString stringValue;
    if (value)
        stringValue = QString::number(*value);
    toResource()->setProperty(kRotationKey, stringValue);
}

Ptz::Capabilities QnMediaResource::getPtzCapabilities(ptz::Type ptzType) const
{
    switch (ptzType)
    {
        case ptz::Type::operational:
            return Ptz::Capabilities(toResource()->getProperty(
                ResourcePropertyKey::kPtzCapabilities).toInt());

        case ptz::Type::configurational:
            return Ptz::Capabilities(toResource()->getProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities).toInt());
        default:
            NX_ASSERT(false, "Wrong ptz type, we should never be here");
            return Ptz::NoPtzCapabilities;
    }
}

bool QnMediaResource::hasAnyOfPtzCapabilities(
    Ptz::Capabilities capabilities,
    ptz::Type ptzType) const
{
    return getPtzCapabilities(ptzType) & capabilities;
}

void QnMediaResource::setPtzCapabilities(
    Ptz::Capabilities capabilities,
    ptz::Type ptzType)
{
    switch (ptzType)
    {
        case ptz::Type::operational:
        {
            // TODO: #sivanov Change the storage type of PTZ capabilities to serialized lexical.
            toResource()->setProperty(
                ResourcePropertyKey::kPtzCapabilities,
                QString::number((int) capabilities));
            break;
        }
        case ptz::Type::configurational:
        {
            // TODO: #sivanov Change the storage type of PTZ capabilities to serialized lexical.
            toResource()->setProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities,
                QString::number((int) capabilities));
            break;
        }
        default:
            NX_ASSERT(false, "Wrong ptz type, we should never be here");
    }

}

void QnMediaResource::setPtzCapability(
    Ptz::Capabilities capability,
    bool value,
    ptz::Type ptzType)
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
