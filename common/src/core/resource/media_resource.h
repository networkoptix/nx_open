#ifndef QN_MEDIA_RESOURCE_H
#define QN_MEDIA_RESOURCE_H

#include <QMap>
#include <QSize>
#include "resource.h"
#include "resource_media_layout.h"
#include "utils/common/from_this_to_shared.h"
#include "fisheye/fisheye_common.h"

class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnResourceAudioLayout;

enum QnStreamQuality {
    QnQualityLowest,
    QnQualityLow,
    QnQualityNormal,
    QnQualityHigh,
    QnQualityHighest,
    QnQualityPreSet,
    QnQualityNotDefined
};

enum QnSecondaryStreamQuality 
{ 
    SSQualityLow, 
    SSQualityMedium, 
    SSQualityHigh, 
    SSQualityNotDefined
};


QString QnStreamQualityToString(QnStreamQuality value);
QnStreamQuality QnStreamQualityFromString( const QString& str );

/*!
    \note Derived class MUST call \a initMediaResource() just after object instanciation
*/
class QnMediaResource
{
public:

    QnMediaResource();
    virtual ~QnMediaResource();

    // size - is size of one channel; we assume all channels have the same size
    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const { return QnQualityNormal; }

    // returns one image best for such time
    // in case of live video time should be ignored
    virtual QImage getImage(int channel, QDateTime time, QnStreamQuality quality) const;

    // resource can use DataProvider for addition info (optional)
    virtual const QnResourceVideoLayout* getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0);
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0);

    virtual const QnResource* toResource() const = 0;
    virtual QnResource* toResource() = 0;
    virtual const QnResourcePtr toResourcePtr() const = 0;
    virtual QnResourcePtr toResourcePtr() = 0;

    virtual bool isFisheye() const;
    DewarpingParams getDewarpingParams() const;
    void setDewarpingParams(const DewarpingParams& params);


    Qn::PtzCapabilities getPtzCapabilities() const;
    bool hasPtzCapabilities(Qn::PtzCapabilities capabilities) const;
    void setPtzCapabilities(Qn::PtzCapabilities capabilities);
    void setPtzCapability(Qn::PtzCapabilities capability, bool value);

protected:
    QnCustomResourceVideoLayout* m_customVideoLayout;
    DewarpingParams m_devorpingParams;

    void initMediaResource();
};

#endif // QN_MEDIA_RESOURCE_H
