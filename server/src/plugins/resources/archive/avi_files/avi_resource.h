#ifndef avi_device_h_1827
#define avi_device_h_1827

#include "../abstract_archive_resource.h"

class QnStreamDataProvider;

class CLAviDevice : public CLAbstractArchiveDevice
{
public:
	CLAviDevice(const QString& file);
	~CLAviDevice();

	virtual QnStreamDataProvider* getDeviceStreamConnection();
	virtual QString toString() const;

protected:

};

#endif //avi_device_h_1827
