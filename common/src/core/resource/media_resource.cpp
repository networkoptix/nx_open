#include "media_resource.h"

#include <QtGui/QImage>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>
#include <utils/serialization/lexical.h>

#include "resource_media_layout.h"
#include "core/dataprovider/abstract_streamdataprovider.h"

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
            //: Short for 'Undefined'
            return tr("-");
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
QnConstResourceVideoLayoutPtr QnMediaResource::getVideoLayout(const QnAbstractStreamDataProvider *dataProvider) const
{
    QMutexLocker lock(&m_layoutMutex);

#ifdef ENABLE_DATA_PROVIDERS
    if (dataProvider) {
        QnConstResourceVideoLayoutPtr providerLayout = dataProvider->getVideoLayout();
        if (providerLayout)
            return providerLayout;
    }
#endif //ENABLE_DATA_PROVIDERS

    QVariant val;
    toResource()->getParam(QLatin1String("VideoLayout"), val, QnDomainMemory);
    QString strVal = val.toString();
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

void QnMediaResource::setCustomVideoLayout(QnCustomResourceVideoLayoutPtr newLayout)
{
    QMutexLocker lock(&m_layoutMutex);
    m_customVideoLayout = newLayout;
    toResource()->setParam(QLatin1String("VideoLayout"), newLayout->toString(), QnDomainMemory);
}

static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr QnMediaResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return audioLayout;
}

void QnMediaResource::initMediaResource()
{
    toResource()->addFlags(Qn::media);
}

QnMediaDewarpingParams QnMediaResource::getDewarpingParams() const {
    return m_dewarpingParams;
}


void QnMediaResource::setDewarpingParams(const QnMediaDewarpingParams& params) {
    if (m_dewarpingParams == params)
        return;

    m_dewarpingParams = params;
    emit toResource()->mediaDewarpingParamsChanged(this->toResourcePtr());
}

void QnMediaResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>&modifiedFields)
{
    QnMediaResourcePtr other_casted = qSharedPointerDynamicCast<QnMediaResource>(other);
    if (other_casted) {
        if (m_dewarpingParams != other_casted->m_dewarpingParams) {
            m_dewarpingParams = other_casted->m_dewarpingParams;
            modifiedFields << "mediaDewarpingParamsChanged";
        }
    }
}

QString QnMediaResource::customAspectRatioKey() {
    return lit("overrideAr");
}

QString QnMediaResource::dontRecordPrimaryStreamKey() {
    return lit("dontRecordPrimaryStream");
}

QString QnMediaResource::dontRecordSecondaryStreamKey() {
    return lit("dontRecordSecondaryStream");
}

QString QnMediaResource::rtpTransportKey() {
    return lit("rtpTransport");
}

QString QnMediaResource::dynamicVideoLayoutKey() {
    return lit("dynamicVideoLayout");
}

QString QnMediaResource::motionStreamKey() {
    return lit("motionStream");
}

QString QnMediaResource::rotationKey() {
    return lit("rotation");
}
