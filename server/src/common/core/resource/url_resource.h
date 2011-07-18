#ifndef file_device_h_227
#define file_device_h_227

#include "resource.h"

class QnAbstractMediaStreamDataProvider;
class QnFileResource;

typedef QSharedPointer<QnFileResource> QnFileResourcePtr;

class QnFileResource : public QnResource 
{

public:
	QnFileResource(QString filename);

    virtual bool equalsTo(const QnResourcePtr other) const;

	DeviceType getDeviceType() const
	{
		return VIDEODEVICE;
	}

	virtual QString toString() const;
	QString getFileName() const;

	virtual QnAbstractMediaStreamDataProvider* getDeviceStreamConnection();
protected:

    QString m_filename;

};

#endif // file_device_h_227
