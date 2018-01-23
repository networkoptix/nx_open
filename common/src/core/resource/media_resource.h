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
class QnCameraUserAttributePool;

namespace Qn {

    // TODO: #Elric move out!

    QString toDisplayString(Qn::StreamQuality value);
    QString toShortDisplayString(Qn::StreamQuality value);
}

/*!
    \note Derived class MUST call \a initMediaResource() just after object instantiation
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
    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = nullptr) const;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = nullptr) const;
    virtual bool hasVideo(const QnAbstractStreamDataProvider* dataProvider = nullptr) const;

    virtual const QnResource* toResource() const = 0;
    virtual QnResource* toResource() = 0;
    virtual const QnResourcePtr toResourcePtr() const = 0;
    virtual QnResourcePtr toResourcePtr() = 0;

    virtual QnMediaDewarpingParams getDewarpingParams() const;
    virtual void setDewarpingParams(const QnMediaDewarpingParams& params);

    // TODO: #dklychkov change to QnAspectRatio in 2.4
    virtual qreal customAspectRatio() const;
    void setCustomAspectRatio(qreal value);
    void clearCustomAspectRatio();

    /** Name of the resource property key intended for the CustomAspectRatio value storage. */
    static QString customAspectRatioKey();
    static QString rotationKey();
    /** Name of the resource property to disable secondary recorder */
    static QString dontRecordPrimaryStreamKey();
    static QString dontRecordSecondaryStreamKey();
    static QString rtpTransportKey();
    static QString panicRecordingKey();
    static QString dynamicVideoLayoutKey();
    static QString motionStreamKey();

    static QString primaryStreamValue();
    static QString secondaryStreamValue();
    static QString edgeStreamValue();

    static QnConstResourceVideoLayoutPtr getDefaultVideoLayout();

protected:
    void initMediaResource();
    QnCameraUserAttributePool* userAttributesPool() const;
protected:
    mutable QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    //QnMediaDewarpingParams m_dewarpingParams;

private:
    mutable QString m_cachedLayout;
    mutable QnMutex m_layoutMutex;
    mutable boost::optional<bool> m_hasVideo;
};

#endif // QN_MEDIA_RESOURCE_H
