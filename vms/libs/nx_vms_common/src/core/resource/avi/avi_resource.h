// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/media/config.h>
#include <nx/media/meta_data_packet.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <utils/common/aspect_ratio.h>

#include "avi_archive_metadata.h"

class QnArchiveStreamReader;
class QnAviArchiveDelegate;

class NX_VMS_COMMON_API QnAviResource:
    public QnAbstractArchiveResource
{
    Q_OBJECT
    using base_type = QnAbstractArchiveResource;

public:
    QnAviResource(const QString& file);
    ~QnAviResource();

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);

    virtual QString toString() const;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;
    virtual AudioLayoutConstPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;

    void setStorage(const QnStorageResourcePtr& storage);
    QnStorageResourcePtr getStorage() const;

    void setMotionBuffer(const QnMetaDataLightVector& data, int channel);
    const QnMetaDataLightVector& getMotionBuffer(int channel) const;

    /* Set item time zone offset in ms */
    void setTimeZoneOffset(qint64 value);

    /* Return item time zone offset in ms */
    qint64 timeZoneOffset() const;
    virtual QnAviArchiveDelegate* createArchiveDelegate() const;
    virtual bool hasVideo(const QnAbstractStreamDataProvider* dataProvider) const override;

    /**
     * @brief imageAspecRatio Returns aspect ratio for image
     * @return Valid aspect ratio if resource is image
     */
    QnAspectRatio imageAspectRatio() const;

    bool hasAviMetadata() const;
    const QnAviArchiveMetadata& aviMetadata() const;
    void setAviMetadata(const QnAviArchiveMetadata& value);

    virtual nx::vms::api::dewarping::MediaData getDewarpingParams() const override;
    virtual void setDewarpingParams(const nx::vms::api::dewarping::MediaData& params) override;

    virtual QnAspectRatio customAspectRatio() const override;
    virtual std::optional<int> forcedRotation() const override;

    /** Returns true if the entity is contained inside a layout file. */
    bool isEmbedded() const;
private:
    QnStorageResourcePtr m_storage;
    QnMetaDataLightVector m_motionBuffer[CL_MAX_CHANNELS];
    qint64 m_timeZoneOffset;
    QnAspectRatio m_imageAspectRatio;
    std::optional<QnAviArchiveMetadata> m_aviMetadata;
    mutable std::optional<bool> m_hasVideo;
    mutable QnConstResourceVideoLayoutPtr m_videoLayout;
    std::optional<int> m_previousRotation;
};
