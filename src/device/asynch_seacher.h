#ifndef cl_asynch_device_sarcher_h_423
#define cl_asynch_device_sarcher_h_423

#include <QThread>
#include "device.h"
#include <QList>
#include <QMutex>
#include "../network/netstate.h"

class CLDeviceServer;



class CLDiviceSeracher : public QThread
{
	typedef QList<CLDeviceServer*> ServerList;

public:
	CLDiviceSeracher(bool allow_change_ip = true);
	~CLDiviceSeracher();
	
	// this function returns only new devices( not in all_devices list);
	CLDeviceList result();
	void addDeviceServer(CLDeviceServer* serv);

	CLDeviceList& getAllDevices()
	{
		return all_devices;
	}

	CLNetState& getNetState()
	{
		return m_netState;
	}

	QMutex all_devices_mtx; // this mutex must be used if deal with all_devices

protected:
	virtual void run();
private:
	// new here means any device BUT any from all_devices with READY flag 
	CLDeviceList findNewDevices(bool allow_to_change_ip, bool& ip_finished);

	// this function will modify CLDeviceList( conflict flag)
	// it only checks obvious conflicts 
	// obvious - means that there are more than one device this such IP;
	bool checkObviousConflicts(CLDeviceList& lst);

	// moves devices from from to to with status&mask = value
	// to move all devices use mask = 0 value = 0
	static void fromListToList(CLDeviceList& from, CLDeviceList& to, int mask, int value);

	// this function will mark cams from list as CONFLICTING if conflicting return true
	static void markConflictingDevices(CLDeviceList& lst, int threads);

private:

	CLDeviceList resolveUnknown_helper(CLDeviceList& lst);

	CLDeviceList m_result;
	bool m_allow_change_ip;

	ServerList m_servers;
	QMutex m_servers_mtx;

	
	CLDeviceList all_devices; // this list of ever found devices.
	
	CLNetState m_netState;
};

#endif //cl_asynch_device_sarcher_h_423
