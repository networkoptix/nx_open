#ifndef CLDeveceCommandProcessor_h_1207
#define CLDeveceCommandProcessor_h_1207

#include "../data/dataprocessor.h"

class CLDevice;

class CLDeviceCommand : public CLAbstractData
{
public:
	CLDeviceCommand(CLDevice* device);
	virtual ~CLDeviceCommand();
	virtual void execute() = 0;
protected:
	CLDevice* m_device;
	
};


class CLDeviceCommandProcessor : public CLAbstractDataProcessor
{
public:
	CLDeviceCommandProcessor();
	~CLDeviceCommandProcessor();

	virtual void processData(CLAbstractData* data);
	

protected:
	
private:
};


#endif //CLDeveceCommandProcessor_h_1207
