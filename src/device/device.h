#ifndef device_h_1051
#define device_h_1051

#include "param.h"
#include "device_command_processor.h"
#include "../base/expensiveobject.h"


struct CLDeviceStatus
{
	enum {NOT_IN_SUBNET = 1, CONFLICTING = 2, IP_LOCKED = 4, READY = 8, NOT_LOCAL = 16, ALL = 0xff};
	CLDeviceStatus():
	status(0)
	{

	};

	quint32 checkFlag(int flag) const
	{
		return status & flag;
	}

	void setFlag(int flag)
	{
		status  |= flag;
	}



	void removeFlag(int flag)
	{
		status &= (~flag);
	}



	quint32 status;

};



class CLDeviceCommand;
class CLStreamreader;
class CLDeviceVideoLayout;


// this class and inherited must be very light to create 
class CLDevice;
typedef QMap<QString, CLDevice*>  CLDeviceList;

static int inst = 0;

class CLDevice : public CLRefCounter
{
	
public:

	enum {NETWORK = 0x00000001, NVR = 0x00000002, SINGLE_SHOT = 0x00000004, ARCHIVE = 0x00000008};

	CLDevice();


	virtual ~CLDevice();

	// each device has unique( over the world) ID; 
	// good example for network device could be MAC address
	QString getUniqueId() const;
	void setUniqueId(const QString& id);

	//Name is class of the devices. like AV2105; => arecontvision 2 megapixel H.264 camera; or "exacq nvr divece"
	QString getName() const;
	void setName(const QString& name);

	// full name of the device; taking into account some properties( like AV2105DN - day/night camera)
	virtual QString getFullName() const;

	// each device has type; for example AV divece has NETWORK
	bool checkDeviceTypeFlag(unsigned long flag) const;
	void addDeviceTypeFlag(unsigned long flag);
	void removeDeviceTypeFlag(unsigned long flag);

	virtual QString toString() const;

	CLDeviceStatus& getStatus() ;
	void  setStatus(const CLDeviceStatus& status) ;



	// return true if no error
	// if (resynch) then ignore synchronized flag
	virtual bool getParam(const QString& name, CLValue& val, bool resynch = false);

	// return true if no error
	// if (resynch) then ignore synchronized flag
	// this is synch function ( will not return value until network part is done)
	virtual bool setParam(const QString& name, const CLValue& val);

	// same as setParam but but returns immediately;
	// this function leads setParam invoke. so no need to make it virtual
	bool setParam_asynch(const QString& name, const CLValue& val);

	// return false if not accessible 
	virtual bool getBaseInfo(){return true;};

	// executing command 
	virtual bool executeCommand(CLDeviceCommand* command){return true;};

	CLParamList& getDevicePramList();// returns params that can be changed on device level
	const CLParamList& getDevicePramList() const;

	CLParamList& getStreamPramList();// returns params that can be changed on stream level 
	const CLParamList& getStreamPramList() const;


	virtual CLStreamreader* getDeviceStreamConnection() = 0;

	// after setVideoLayout is called device is responsable for the destroying the layout 
	void setVideoLayout(CLDeviceVideoLayout* layout);

	const CLDeviceVideoLayout* getVideoLayout() const;

public:

	
	static QStringList supportedDevises();


	// this function will call getBaseInfo for each device with conflicted = false in multiple thread
	// lst - device list; threads - number of threads 
	static void getDevicesBasicInfo(CLDeviceList& lst, int threads);


	// will extend the first one and remove all elements from the second one
	static void mergeLists(CLDeviceList& first, CLDeviceList second);

	static void deleteDevices(CLDeviceList& lst);

	static void addReferences(CLDeviceList& lst);

	

	static void startCommandProc() {m_commanproc.start();};
	static void stopCommandProc() {m_commanproc.stop();};
	static int commandProcQueSize() {return m_commanproc.queSize();}


protected:

	typedef QMap<QString, CLParamList > LL;
	static LL static_device_list; // list of all supported devices params list
	static LL static_stream_list; // list of all supported streams params list

	// this is thread to process commands like setparam
	
	static CLDeviceCommandProcessor m_commanproc;

protected:

	mutable CLParamList m_deviceParamList;
	mutable CLParamList m_streamParamList;



	QString m_name; // this device model like AV2105 or AV2155dn 

	QString m_uniqueid; //+
	QString m_description;

	CLDeviceStatus m_status;

	unsigned long m_deviceTypeFlags;

	mutable CLDeviceVideoLayout* m_videolayout;
	

};


#endif