#include "media_resource.h"

#include <QtGui/QImage>
#include <QtCore/QCoreApplication>

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


class QnStreamQualityStrings {
    Q_DECLARE_TR_FUNCTIONS(QnStreamQualityStrings);
public:
    static QString displayString(Qn::StreamQuality value) {
        switch(value) {
        case Qn::QualityLowest:       return tr("Lowest");
        case Qn::QualityLow:          return tr("Low");
        case Qn::QualityNormal:       return tr("Medium");
        case Qn::QualityHigh:         return tr("High");
        case Qn::QualityHighest:      return tr("Best");
        case Qn::QualityPreSet:       return tr("Preset");
        case Qn::QualityNotDefined:   return tr("Undefined");
        default:
            qnWarning("Invalid stream quality value '%1'.", static_cast<int>(value));
            return QString();
        }
    }

    static QString shortDisplayString(Qn::StreamQuality value) {
        /* Note that '//:' are comments for translators. */
        switch(value) {
        case Qn::QualityLowest:
            //: Short for 'Lowest'
            return tr("Lst");
        case Qn::QualityLow:
            //: Short for 'Low'
            return tr("Lo");
        case Qn::QualityNormal:
            //: Short for 'Medium'
            return tr("Me");
        case Qn::QualityHigh:
            //: Short for 'High'
            return tr("Hi");
        case Qn::QualityHighest:
            //: Short for 'Best'
            return tr("Bst");
        case Qn::QualityPreSet:
            //: Short for 'Preset'
            return tr("Ps");
        case Qn::QualityNotDefined:
            return lit("-");
        default:
            qnWarning("Invalid stream quality value '%1'.", static_cast<int>(value));
            return QString();
        }
    }
};

QString Qn::toDisplayString(Qn::StreamQuality value) {
    return QnStreamQualityStrings::displayString(value);
}

QString Qn::toShortDisplayString(Qn::StreamQuality value) {
    return QnStreamQualityStrings::shortDisplayString(value);
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

bool QnMediaResource::hasVideo(const QnAbstractStreamDataProvider* dataProvider) const
{
    Q_UNUSED(dataProvider);
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

qreal QnMediaResource::customAspectRatio() const
{
    if (!this->toResource()->hasProperty(::customAspectRatioKey))
        return noCustomAspectRatio;

    bool ok = true;
    qreal value = this->toResource()->getProperty(::customAspectRatioKey).toDouble(&ok);
    if (!ok || qIsNaN(value) || qIsInf(value) || value < 0)
        return noCustomAspectRatio;

    return value;
}

void QnMediaResource::setCustomAspectRatio(qreal value) {
    if (qIsNaN(value) || qIsInf(value) || value < 0 || qFuzzyEquals(value, noCustomAspectRatio))
        clearCustomAspectRatio();
    else
        this->toResource()->setProperty(::customAspectRatioKey, QString::number(value));
}

void QnMediaResource::clearCustomAspectRatio() {
    this->toResource()->setProperty(::customAspectRatioKey, QString());
}

QString QnMediaResource::customAspectRatioKey() {
    return ::customAspectRatioKey;
}

QString QnMediaResource::dontRecordPrimaryStreamKey() {
    return ::dontRecordPrimaryStreamKey;
}

QString QnMediaResource::dontRecordSecondaryStreamKey() {
    return ::dontRecordSecondaryStreamKey;
}

QString QnMediaResource::rtpTransportKey() {
    return ::rtpTransportKey;
}

QString QnMediaResource::panicRecordingKey() {
    return ::panicRecordingKey;
}

QString QnMediaResource::dynamicVideoLayoutKey() {
    return ::dynamicVideoLayoutKey;
}

QString QnMediaResource::motionStreamKey() {
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
