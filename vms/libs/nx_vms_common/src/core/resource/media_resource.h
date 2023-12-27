// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <core/ptz/ptz_constants.h>
#include <core/resource/resource.h>
#include <core/resource/resource_media_layout_fwd.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/common/ptz/type.h>
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

    virtual const QnResource* toResource() const = 0;
    virtual QnResource* toResource() = 0;
    virtual const QnResourcePtr toResourcePtr() const = 0;
    virtual QnResourcePtr toResourcePtr() = 0;

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

    /**
        Control PTZ flags. Better place is mediaResource but no signals allowed in MediaResource
    */
    Ptz::Capabilities getPtzCapabilities(
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational) const;

    /** Check if camera has any of provided capabilities. */
    bool hasAnyOfPtzCapabilities(
        Ptz::Capabilities capabilities,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational) const;
    void setPtzCapabilities(
        Ptz::Capabilities capabilities,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational);
    void setPtzCapability(
        Ptz::Capabilities capability,
        bool value,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational);

    bool canSwitchPtzPresetTypes() const;

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
