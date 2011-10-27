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

    virtual QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
	virtual QString toString() const;
    virtual int getStreamDataProvidersMaxAmount() const { return INT_MAX; }
};

typedef QSharedPointer<QnAviResource> QnAviResourcePtr;

#endif //avi_device_h_1827
