#ifndef cl_asynch_device_sarcher_h_423
#define cl_asynch_device_sarcher_h_423

#include "device.h"
#include "../network/netstate.h"
#include "network/nettools.h"

class CLDeviceServer;

// this class just searches for new devices
// it uses others proxy
// it will be moved to recorder I guess 
class CLDeviceSearcher : public QThread
{
    Q_OBJECT

	typedef QList<CLDeviceServer*> ServerList;

public:
	CLDeviceSearcher();
	~CLDeviceSearcher();

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

    void releaseDevices();
    void addDevice(CLDevice* device);
    void removeDeviceUnlocked(const QString& deviceId);
    void removeRemoved(QStringList folders, CLDeviceList devices);

signals:
    void newNetworkDevices();

protected:
	virtual void run();

private:
	// new here means any device BUT any from all_devices with READY flag 
	CLDeviceList findNewDevices(bool& ip_finished);

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

	void resovle_conflicts(CLDeviceList& device_list, CLIPList& busy_list, bool& ip_finished);

	CLDeviceList resolveUnknown_helper(CLDeviceList& lst);

private:
	CLDeviceList m_result;
	ServerList m_servers;
	QMutex m_servers_mtx;

	CLDeviceList all_devices; // this list of ever found devices.

    //Directory name -> children device IDs mapping.
    typedef QMap<QString, QSet<QString> > DirectoryEntries;
    DirectoryEntries m_directoryEntries;

	CLNetState m_netState;
};

#endif //cl_asynch_device_sarcher_h_423
