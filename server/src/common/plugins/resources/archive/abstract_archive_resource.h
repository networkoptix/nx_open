#ifndef abstract_archive_device_h1838
#define abstract_archive_device_h1838

#include "resource/resource.h"

class QnAbstractArchiveResource : public QnResource
{
public:
	QnAbstractArchiveResource();
	~QnAbstractArchiveResource();
	virtual DeviceType getDeviceType() const;

};

#endif //abstract_archive_device_h1838
