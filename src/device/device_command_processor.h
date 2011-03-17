#ifndef CLDeveceCommandProcessor_h_1207
#define CLDeveceCommandProcessor_h_1207

#include "../data/dataprocessor.h"

class CLDevice;

class CLDeviceCommand : public CLAbstractData
{
public:
	CLDeviceCommand(CLDevice* device);
	virtual ~CLDeviceCommand();
	CLDevice* getDevice() const;
	virtual void execute() = 0;
protected:
	CLDevice* m_device;

};

class CLDeviceCommandProcessor : public CLAbstractDataProcessor
{
public:
	CLDeviceCommandProcessor();
	~CLDeviceCommandProcessor();

	virtual void putData(CLAbstractData* data);

	virtual void clearUnprocessedData();

	bool hasSuchDeviceInQueue(CLDevice* dev) const;

protected:
	virtual void processData(CLAbstractData* data);
private:
	QMap<CLDevice*, unsigned int> mDevicesQue;
	mutable QMutex m_cs;
};

#endif //CLDeveceCommandProcessor_h_1207
