#ifndef QN_AVI_RESOURCE_H
#define QN_AVI_RESOURCE_H

#include <nx/streaming/abstract_archive_resource.h>
#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <utils/common/aspect_ratio.h>

#include "avi_archive_metadata.h"

class QnArchiveStreamReader;
class QnAviArchiveDelegate;

class QnAviResource: public QnAbstractArchiveResource
{
    Q_OBJECT
    using base_type = QnAbstractArchiveResource;

public:
    QnAviResource(const QString& file, QnCommonModule* commonModule);
    ~QnAviResource();

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);

    virtual QString toString() const;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;

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

    virtual QnMediaDewarpingParams getDewarpingParams() const override;
    virtual void setDewarpingParams(const QnMediaDewarpingParams& params) override;

    virtual QnAspectRatio customAspectRatio() const override;

    /** Returns true if the entity is contained inside a layout file. */
    bool isEmbedded() const;

private:
    QnStorageResourcePtr m_storage;
    QnMetaDataLightVector m_motionBuffer[CL_MAX_CHANNELS];
    qint64 m_timeZoneOffset;
    QnAspectRatio m_imageAspectRatio;
    boost::optional<QnAviArchiveMetadata> m_aviMetadata;
    mutable boost::optional<bool> m_hasVideo;
    mutable QnConstResourceVideoLayoutPtr m_videoLayout;
};

#endif // QN_AVI_RESOURCE_H
