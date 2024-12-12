// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <core/resource/resource.h>
#include <core/resource/resource_media_layout_fwd.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <utils/common/aspect_ratio.h>

class QnAbstractStreamDataProvider;

/**
 * Represents a camera or a local video file.
 * This class is baseless to prevent diamond inheritance. Its descendants are QnVirtualCameraResource
 * or local QnAbstractArchiveResource based.
 * \note Derived class MUST call \a initMediaResource() just after object instantiation
*/
class NX_VMS_COMMON_API QnMediaResource: public QnResource
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
    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = nullptr);
    virtual AudioLayoutConstPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = nullptr) const;
    virtual bool hasVideo(const QnAbstractStreamDataProvider* dataProvider = nullptr) const = 0;

    virtual nx::vms::api::dewarping::MediaData getDewarpingParams() const;
    virtual void setDewarpingParams(const nx::vms::api::dewarping::MediaData& params);

    virtual QnAspectRatio customAspectRatio() const;
    void setCustomAspectRatio(const QnAspectRatio& value);
    void clearCustomAspectRatio();

    /**
     * Returns default rotation.
     * The class should be refactored and this function moved out.
     */
    virtual std::optional<int> forcedRotation() const;

    void setForcedRotation(std::optional<int> value);

    /** Name of the resource property key intended for the CustomAspectRatio value storage. */
    static QString customAspectRatioKey();

    static QString rtpTransportKey();
    static QString panicRecordingKey();
    static QString dynamicVideoLayoutKey();

    static QnConstResourceVideoLayoutPtr getDefaultVideoLayout();

protected:
    void initMediaResource();

    mutable nx::Mutex m_attributesMutex;
    nx::vms::api::CameraAttributesData m_userAttributes;
    mutable std::optional<nx::vms::api::dewarping::MediaData> m_cachedDewarpingParams;

protected:
    mutable QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    mutable nx::Mutex m_layoutMutex;

    static const QString kRotationKey;

private:
    mutable QString m_cachedLayout;
};
