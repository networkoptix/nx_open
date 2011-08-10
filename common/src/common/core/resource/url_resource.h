#ifndef file_device_h_227
#define file_device_h_227

#include "resource.h"


class QnURLResource;

typedef QSharedPointer<QnURLResource> QnUrlResourcePtr;

class QnURLResource : virtual public QnResource 
{

public:
    QnURLResource();
	QnURLResource(const QString& url);

    virtual bool equalsTo(const QnResourcePtr other) const;
	virtual QString toString() const;

	QString getUrl() const;
    void setUrl(const QString& url);
protected:
    QString m_url;
};

#endif // file_device_h_227
