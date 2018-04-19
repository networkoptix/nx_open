#ifndef QN_MEDIA_RESOURCE_H
#define QN_MEDIA_RESOURCE_H

#include <QtCore/QMap>
#include <QtCore/QSize>

#include "resource.h"
#include "resource_media_layout.h"

#include <core/ptz/ptz_constants.h>
#include <core/ptz/media_dewarping_params.h>

#include <utils/common/aspect_ratio.h>

class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnResourceAudioLayout;
class QnCameraUserAttributePool;

/*!
    \note Derived class MUST call \a initMediaResource() just after object instantiation
*/
class QnMediaResource
{
public:
    QnMediaResource();
    virtual ~QnMediaResource();

    // size - is size of one channel; we assume all channels have the same size
    virtual Qn::StreamQuality getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const;

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

    virtual QnAspectRatio customAspectRatio() const;
    void setCustomAspectRatio(const QnAspectRatio& value);
    void clearCustomAspectRatio();

    /**
        Control PTZ flags.
    */
    Ptz::Capabilities getPtzCapabilities() const;

    /** Check if camera has any of provided capabilities. */
    bool hasAnyOfPtzCapabilities(Ptz::Capabilities capabilities) const;
    void setPtzCapabilities(Ptz::Capabilities capabilities);
    void setPtzCapability(Ptz::Capabilities capability, bool value);

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
