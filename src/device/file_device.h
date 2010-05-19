#ifndef file_device_h_227
#define file_device_h_227

#include "../network/netstate.h"
#include "device.h"



class CLFileDevice : public CLDevice 
{
	
public:
	CLFileDevice(QString filename);
	virtual QString toString() const;
	QString getFileName() const;


	virtual CLStreamreader* getDeviceStreamConnection();
protected:
	
	
};


#endif // file_device_h_227