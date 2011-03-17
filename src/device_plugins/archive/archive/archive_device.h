#ifndef archive_device_h1854
#define archive_device_h1854

#include "..\abstract_archive_device.h"

class CLArchiveDevice : public CLAbstractArchiveDevice
{
public:
	CLArchiveDevice(const QString& arch_path);
	~CLArchiveDevice();

	virtual CLStreamreader* getDeviceStreamConnection();
protected:

	void readdescrfile();
};

#endif //archive_device_h1854