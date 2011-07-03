#ifndef device_h_1051
#define device_h_1051

#include "common/expensiveobject.h"
#include "common/associativearray.h"
#include "resource_param.h"
#include "datapacket/datapacket.h"
#include "resourcecontrol/resource_command_consumer.h"


struct QnResourceStatus
{
	enum {NOT_IN_SUBNET = 1, CONFLICTING = 2, IP_LOCKED = 4, READY = 8, NOT_LOCAL = 16, ALL = 0xff};
	QnResourceStatus()
        : status(0)
	{
	}

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

class QnResourceCommand;
class QnStreamDataProvider;
class QnVideoResoutceLayout;

// this class and inherited must be very light to create 
class QnResource;
typedef QMap<QString, QnResource*>  QnResourceList;

class QnResource : public CLRefCounter
{
public:
	enum {NETWORK = 0x00000001, NVR = 0x00000002, SINGLE_SHOT = 0x00000004, ARCHIVE = 0x00000008, RECORDED = 0x00000010};

	enum DeviceType {RECORDER, VIDEODEVICE};

	QnResource();

	virtual ~QnResource();

	virtual DeviceType getDeviceType() const = 0;

	void setParentId(QString parent);
	QString getParentId() const;

	// each device has unique( over the world) ID; 
	// good example for network device could be MAC address
	QString getUniqueId() const;
	void setUniqueId(const QString& id);

	//Name is class of the devices. like 2105DN; => arecontvision 2 megapixel H.264 day night camera; or "exacq nvr divece"
	virtual QString getName() const;
	void setName(const QString& name);

	// each device has type; for example AV divece has NETWORK
	bool checkDeviceTypeFlag(unsigned long flag) const;
	void addDeviceTypeFlag(unsigned long flag);
	void removeDeviceTypeFlag(unsigned long flag);

	virtual QString toString() const;

	QnResourceStatus& getStatus() ;
	void  setStatus(const QnResourceStatus& status) ;

	// return true if no error
	// if (resynch) then ignore synchronized flag
	virtual bool getParam(const QString& name, QnValue& val, bool resynch = false);

	// return true if no error
	// if (resynch) then ignore synchronized flag
	// this is synch function ( will not return value until network part is done)
	virtual bool setParam(const QString& name, const QnValue& val);

	// same as setParam but but returns immediately;
	// this function leads setParam invoke. so no need to make it virtual
	bool setParam_asynch(const QString& name, const QnValue& val);

	// return false if not accessible 
	virtual bool getBaseInfo(){return true;};

	// this function is called by stream reader before start read;
	// on in case of connection lost and restored 
	virtual void onBeforeStart(){};

	// executing command 
	virtual bool executeCommand(QnResourceCommand* /*command*/){return true;};

	QnParamList& getDeviceParamList();// returns params that can be changed on device level
	const QnParamList& getDeviceParamList() const;

	QnParamList& getStreamParamList();// returns params that can be changed on stream level 
	const QnParamList& getStreamParamList() const;

	virtual QnStreamDataProvider* getDeviceStreamConnection() = 0;

	// after setVideoLayout is called device is responsable for the destroying the layout 
	void setVideoLayout(QnVideoResoutceLayout* layout);

	const QnVideoResoutceLayout* getVideoLayout() const;

public:
	static QStringList supportedDevises();

	// this function will call getBaseInfo for each device with conflicted = false in multiple thread
	// lst - device list; threads - number of threads 
	static void getDevicesBasicInfo(QnResourceList& lst, int threads);

	// will extend the first one and remove all elements from the second one
	static void mergeLists(QnResourceList& first, QnResourceList second);

	static void deleteDevices(QnResourceList& lst);

	static void addReferences(QnResourceList& lst);

	static void startCommandProc() {m_commanproc.start();};
	static void stopCommandProc() {m_commanproc.stop();};
    static void addCommandToProc(QnAbstractDataPacketPtr data) {m_commanproc.putData(data);};
	static int commandProcQueSize() {return m_commanproc.queueSize();}
	static bool commandProchasSuchDeviceInQueue(QnResource* dev) {return m_commanproc.hasSuchDeviceInQueue(dev);}

protected:
	typedef QMap<QString, QnParamList > LL;
	static LL static_device_list; // list of all supported devices params list
	static LL static_stream_list; // list of all supported streams params list

	// this is thread to process commands like setparam

	static QnResourceCommandProcessor m_commanproc;

protected:
	mutable QnParamList m_deviceParamList;
	mutable QnParamList m_streamParamList;

	QString m_name; // this device model like AV2105 or AV2155dn 
	QString m_uniqueId; //+
	QString m_description;

	QnResourceStatus m_status;

	unsigned long m_deviceTypeFlags;

	mutable QnVideoResoutceLayout* m_videolayout;

	QString m_parentId;
};

#endif
