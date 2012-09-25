#ifndef QN_AVI_RESOURCE_H
#define QN_AVI_RESOURCE_H

#include "../abstract_archive_resource.h"
#include <QSharedPointer>
#include "core/datapacket/media_data_packet.h"

class QnArchiveStreamReader;

class QnAviResource : public QnAbstractArchiveResource
{
    Q_OBJECT;

public:
    QnAviResource(const QString& file);
    ~QnAviResource();

    void deserialize(const QnResourceParameters&);

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
    virtual QString toString() const;

    virtual const QnResourceVideoLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;

    void setStorage(QnStorageResourcePtr);
    QnStorageResourcePtr getStorage() const;

    void setMotionBuffer(const QnMetaDataLightVector& data, int channel);
    const QnMetaDataLightVector& getMotionBuffer(int channel) const;
protected:

private:
    QnStorageResourcePtr m_storage;
    QnMetaDataLightVector m_motionBuffer[CL_MAX_CHANNELS];
};

typedef QnSharedResourcePointer<QnAviResource> QnAviResourcePtr;

#endif // QN_AVI_RESOURCE_H
