#ifndef device_manager_h_1537_
#define device_manager_h_1537_

// this class holds all devices in the system ( a least knows how to get the list )
// it also can give list of devices based on some criteria
#include "..\asynch_seacher.h"
#include "..\device.h"
#include "device_criteria.h"

class CLDevice;
class CLRecorderDevice;

// this class is a buffer. makes searcher to serch cameras and puts it into buffer
class CLDeviceManager : public QObject
{
	Q_OBJECT

private:
	~CLDeviceManager();

	static CLDeviceManager* m_Instance;
	static QString ms_RootDir;
public:
	static void setRootDir( QString rootDir );

	static CLDeviceManager& instance();

	CLDiviceSeracher& getDiveceSercher(); 

	CLDeviceList getDeviceList(CLDeviceCriteria& cr);
	CLDevice* getDeviceById(QString id);

	CLDeviceList getRecorderList();
	CLDevice* getRecorderById(QString id); 

	CLDevice* getArchiveDevice(QString id); 

protected:
	CLDeviceManager();
	void onNewDevices_helper(CLDeviceList devices, QString parentId);

	bool isDeviceMeetCriteria(CLDeviceCriteria& cr, CLDevice* dev) const;

	QStringList subDirList(const QString& abspath) const;
	void addArchiver(QString id);

private:
    bool match_subfilter(QString dec, QString fltr) const;

protected slots:
	void onTimer();
protected:

	QTimer m_timer;
	CLDiviceSeracher m_dev_searcher;
	bool m_firstTime;

	CLDeviceList mRecDevices;
};

#endif //device_manager_h_1537_