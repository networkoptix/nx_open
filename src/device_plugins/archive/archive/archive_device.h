#ifndef archive_device_h1854
#define archive_device_h1854

#include "../abstract_archive_device.h"

class CLArchiveDevice : public CLAbstractArchiveDevice
{
public:
	CLArchiveDevice(const QString& arch_path);
	~CLArchiveDevice();

	virtual CLStreamreader* getDeviceStreamConnection();

    QString toString() const;

    QString originalName() const;
protected:
	void readdescrfile();
protected:
    QString mOriginalName;

};

#endif //archive_device_h1854
