#include "media_resource.h"

#include <QtGui/QImage>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>
#include <utils/common/enum_name_mapper.h>

#include "resource_media_layout.h"

QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING(Qn::StreamQuality, 
    ((Qn::QualityLowest,  "lowest"))
    ((Qn::QualityLow,     "low"))
    ((Qn::QualityNormal,  "normal"))
    ((Qn::QualityHigh,    "high"))
    ((Qn::QualityHighest, "highest"))
    ((Qn::QualityPreSet,  "preset"))
)
Q_GLOBAL_STATIC_WITH_ARGS(QnTypedEnumNameMapper<Qn::StreamQuality>, qn_streamQualityNameMapper_instance, (QnEnumNameMapper::create<Qn::StreamQuality>()))

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

template<>
Qn::StreamQuality Qn::fromString<Qn::StreamQuality>(const QString &string) {
    return qn_streamQualityNameMapper_instance()->value(string, Qn::QualityNotDefined);
}

template<>
QString Qn::toString<Qn::StreamQuality>(Qn::StreamQuality value) {
    return qn_streamQualityNameMapper_instance()->name(value, QString());
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

static std::shared_ptr<QnDefaultResourceVideoLayout> defaultVideoLayout( new QnDefaultResourceVideoLayout() );
QnConstResourceVideoLayoutPtr QnMediaResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    QVariant val;
    toResource()->getParam(QLatin1String("VideoLayout"), val, QnDomainMemory);
    QString strVal = val.toString();
    if (strVal.isEmpty())
    {
        return defaultVideoLayout;
    }
    else {
        if (!m_customVideoLayout)
            m_customVideoLayout = QnCustomResourceVideoLayout::fromString(strVal);
        return m_customVideoLayout;
    }
}

void QnMediaResource::setCustomVideoLayout(QnCustomResourceVideoLayoutPtr newLayout)
{
    //if (!m_customVideoLayout)
        //m_customVideoLayout.reset( new QnCustomResourceVideoLayout(newLayout->size()) );

    m_customVideoLayout = newLayout;
    toResource()->setParam(QLatin1String("VideoLayout"), newLayout->toString(), QnDomainMemory);
}

static std::shared_ptr<QnEmptyResourceAudioLayout> audioLayout( new QnEmptyResourceAudioLayout() );
QnConstResourceAudioLayoutPtr QnMediaResource::getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/)
{
    return audioLayout;
}

void QnMediaResource::initMediaResource()
{
    toResource()->addFlags(QnResource::media);
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

void QnMediaResource::updateInner(QnResourcePtr other, QSet<QByteArray>&)
{
    QnMediaResourcePtr other_casted = qSharedPointerDynamicCast<QnMediaResource>(other);
    if (other_casted)
        m_dewarpingParams = other_casted->m_dewarpingParams;
}
