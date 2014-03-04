#ifndef QN_AVI_RESOURCE_H
#define QN_AVI_RESOURCE_H

#include "../abstract_archive_resource.h"
#include <QSharedPointer>
#include "core/datapacket/media_data_packet.h"

class QnArchiveStreamReader;
class QnAviArchiveDelegate;

class QnAviResource : public QnAbstractArchiveResource
{
    Q_OBJECT;

public:
    QnAviResource(const QString& file);
    ~QnAviResource();

    virtual void deserialize(const QnResourceParameters&) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
    virtual QString toString() const;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;

    void setStorage(QnStorageResourcePtr);
    QnStorageResourcePtr getStorage() const;

    void setMotionBuffer(const QnMetaDataLightVector& data, int channel);
    const QnMetaDataLightVector& getMotionBuffer(int channel) const;

    /* Set item time zone offset in ms */
    void setTimeZoneOffset(qint64 value);

    /* Return item time zone offset in ms */
    qint64 timeZoneOffset() const;
    QnAviArchiveDelegate* createArchiveDelegate() const;

private:
    QnStorageResourcePtr m_storage;
    QnMetaDataLightVector m_motionBuffer[CL_MAX_CHANNELS];
    qint64 m_timeZoneOffset;
};

typedef QnSharedResourcePointer<QnAviResource> QnAviResourcePtr;

#endif // QN_AVI_RESOURCE_H
