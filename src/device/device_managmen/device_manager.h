#ifndef device_manager_h_1537_
#define device_manager_h_1537_


// this class holds all devices in the system ( a least knows how to get the list )
// it also can give list of devices based on some criteria
#include <QObject>
#include <QTimer>
#include "..\asynch_seacher.h"
#include "..\device.h"
class CLDevice;
class CLRecorderDevice;

struct CLDeviceCriteria // later
{
public:
	enum CriteriaType {ALL, NONE};
	CLDeviceCriteria(CriteriaType cr);

	CriteriaType getCriteria() const;

	void setRecorderId(QString id);
	QString getRecorderId() const;


protected:
	CriteriaType mCriteria;
	QString mRecorderId;
};


// this class is a buffer. makes searcher to serch cameras and puts it into buffer
class CLDeviceManager : public QObject
{
	Q_OBJECT
		
public:
	static CLDeviceManager& instance();
	~CLDeviceManager();

	CLDiviceSeracher& getDiveceSercher(); 


	CLDeviceList getDeviceList(CLDeviceCriteria& cr);

	CLDeviceList getRecorderList();
	CLDevice* getRecorderById(QString id); 

protected:

	CLDeviceManager();
	void onNewDevices_helper(CLDeviceList devices);

	bool isDeviceMeetCriteria(CLDeviceCriteria& cr, CLDevice* dev) const;

protected slots:
	void onTimer();
protected:

	QTimer m_timer;
	CLDiviceSeracher m_dev_searcher;
	bool m_firstTime;

	CLRecorderDevice* mRecDevice;
};


#endif //device_manager_h_1537_