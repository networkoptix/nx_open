#ifndef avi_device_h_1827
#define avi_device_h_1827

#include "../abstract_archive_resource.h"



class QnArchiveStreamReader;

class QnAviResource : public QnAbstractArchiveResource
{
public:
	QnAviResource(const QString& file);
	~QnAviResource();

    void deserialize(const QnResourceParameters&);

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
	virtual QString toString() const;
    virtual int getStreamDataProvidersMaxAmount() const { return INT_MAX; }

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0);
};

typedef QSharedPointer<QnAviResource> QnAviResourcePtr;

#endif //avi_device_h_1827
