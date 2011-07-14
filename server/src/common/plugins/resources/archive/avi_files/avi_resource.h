#ifndef avi_device_h_1827
#define avi_device_h_1827

#include "../abstract_archive_resource.h"

class QnMediaStreamDataProvider;
class QnAviResource;

typedef QSharedPointer<QnAviResource> QnAviResourcePtr;

class QnAviResource : public QnAbstractArchiveResource
{
public:
	QnAviResource(const QString& file);
	~QnAviResource();

    QString getUrl() const;

	virtual QnMediaStreamDataProvider* getDeviceStreamConnection();
	virtual QString toString() const;

    virtual bool equalsTo(const QnResourcePtr other) const;

protected:
    
    QString m_url;

};

#endif //avi_device_h_1827
