#ifndef QN_MEDIA_RESOURCE_H
#define QN_MEDIA_RESOURCE_H

#include <QtCore/QMap>
#include <QtCore/QSize>
#include "resource.h"
#include "resource_media_layout.h"
#include "utils/common/from_this_to_shared.h"

#include <core/ptz/media_dewarping_params.h>

class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnResourceAudioLayout;
class MediaStreamCache;
class MediaIndex;

namespace Qn {

    enum StreamQuality {
        QualityLowest,
        QualityLow,
        QualityNormal,
        QualityHigh,
        QualityHighest,
        QualityPreSet,
        QualityNotDefined,

        StreamQualityCount
    };

    enum SecondStreamQuality { 
        SSQualityLow, 
        SSQualityMedium, 
        SSQualityHigh, 
        SSQualityNotDefined,
        SSQualityDontUse
    };

    QString toDisplayString(Qn::StreamQuality value);
    QString toShortDisplayString(Qn::StreamQuality value);

    // TODO: #Elric move out as generic interface
    template<class Enum> Enum fromString(const QString &string);
    template<class Enum> QString toString(Enum value);

    template<> Qn::StreamQuality fromString<Qn::StreamQuality>(const QString &string);
    template<> QString toString<Qn::StreamQuality>(Qn::StreamQuality value);
}

/*!
    \note Derived class MUST call \a initMediaResource() just after object instanciation
*/
class QnMediaResource
{
public:
    QnMediaResource();
    virtual ~QnMediaResource();

    // size - is size of one channel; we assume all channels have the same size
    virtual Qn::StreamQuality getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const { return Qn::QualityNormal; }

    // returns one image best for such time
    // in case of live video time should be ignored
    virtual QImage getImage(int channel, QDateTime time, Qn::StreamQuality quality) const;

    // resource can use DataProvider for addition info (optional)
    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0);
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0);

    void setCustomVideoLayout(QnCustomResourceVideoLayoutPtr newLayout);

    virtual const QnResource* toResource() const = 0;
    virtual QnResource* toResource() = 0;
    virtual const QnResourcePtr toResourcePtr() const = 0;
    virtual QnResourcePtr toResourcePtr() = 0;

    QnMediaDewarpingParams getDewarpingParams() const;
    void setDewarpingParams(const QnMediaDewarpingParams& params);

protected:
    void initMediaResource();
    void updateInner(QnResourcePtr other);

protected:
    QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    QnMediaDewarpingParams m_dewarpingParams;
};

#endif // QN_MEDIA_RESOURCE_H
