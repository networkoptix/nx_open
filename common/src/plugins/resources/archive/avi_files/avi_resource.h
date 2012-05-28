#ifndef QN_AVI_RESOURCE_H
#define QN_AVI_RESOURCE_H

#include "../abstract_archive_resource.h"

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

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;

    void setStorage(QnStorageResourcePtr);
private:
    QnStorageResourcePtr m_storage;
};

typedef QnSharedResourcePointer<QnAviResource> QnAviResourcePtr;

#endif // QN_AVI_RESOURCE_H
