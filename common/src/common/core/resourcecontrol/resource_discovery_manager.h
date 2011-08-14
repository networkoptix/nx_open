#ifndef cl_asynch_device_sarcher_h_423
#define cl_asynch_device_sarcher_h_423

#include <QtCore/QThread>

#include "resource/resource.h"
#include "network/netstate.h"
#include "network/nettools.h"

class QnAbstractResourceSearcher;

// this class just searches for new devices
// it uses others proxy
// it will be moved to recorder I guess
class QnResourceDiscoveryManager : public QThread
{
	typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

public:
	QnResourceDiscoveryManager();
	~QnResourceDiscoveryManager();

	// this function returns only new devices( not in all_devices list);
	QnResourceList result();
	void addDeviceServer(QnAbstractResourceSearcher* serv);

	CLNetState& getNetState()
	{
		return m_netState;
	}

protected:
	virtual void run();
private:
	// returns new resources( not from pool) or updates some in resource pool
	QnResourceList findNewResources(bool& ip_finished);

	// this function will modify CLDeviceList( conflict flag)
	// it only checks obvious conflicts
	// obvious - means that there are more than one device this such IP;
	bool checkObviousConflicts(QnResourceList& lst);

	// moves devices from from to to with status&mask = value
	// to move all devices use mask = 0 value = 0
	static void fromListToList(QnResourceList& from, QnResourceList& to, int mask, int value);

	// this function will mark cams from list as CONFLICTING if conflicting return true
	static void markConflictingResources(QnResourceList& lst, int threads);

private:

	void resovle_conflicts(QnResourceList& device_list, CLIPList& busy_list, bool& ip_finished);

	QnResourceList resolveUnknown_helper(QnResourceList& lst);

private:

	QnResourceList m_result;

	ResourceSearcherList m_searchersList;
	QMutex m_searchersListMtx;

	CLNetState m_netState;
};

#endif //cl_asynch_device_sarcher_h_423
