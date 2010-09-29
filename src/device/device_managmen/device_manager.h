#ifndef device_manager_h_1537_
#define device_manager_h_1537_


// this class holds all devices in the system ( a least knows how to get the list )
// it also can give list of devices based on some criteria
#include <QObject>
#include <QTimer>
#include "..\asynch_seacher.h"
#include "..\device.h"
class CLDevice;

struct CLDeviceCriteria // later
{
public:
	enum CriteriaType {ALL, NONE};
	CriteriaType mCriteria;
};


class CLDeviceManager : public QObject
{
	Q_OBJECT
		
public:
	static CLDeviceManager& instance();
	~CLDeviceManager();

	CLDiviceSeracher& getDiveceSercher(); 


	CLDeviceList getDeviceList(CLDeviceCriteria& cr);

protected:

	CLDeviceManager();
	void onNewDevices_helper(CLDeviceList devices);

protected slots:
	void onTimer();
protected:

	QTimer m_timer;
	CLDiviceSeracher m_dev_searcher;
	bool m_firstTime;
};


#endif //device_manager_h_1537_