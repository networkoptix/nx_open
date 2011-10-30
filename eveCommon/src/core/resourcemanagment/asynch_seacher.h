#ifndef cl_asynch_device_sarcher_h_423
#define cl_asynch_device_sarcher_h_423

#include <QtCore/QThread>
#include "utils/common/longrunnable.h"
#include "utils/network/netstate.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"



class QnAbstractResourceSearcher;

// this class just searches for new resources
// it uses others plugins 
// it puts result into resource pool
class QnResourceDiscoveryManager : public CLLongRunnable
{
    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

public:
    
    ~QnResourceDiscoveryManager();

    static QnResourceDiscoveryManager& instance(); 

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceServer(QnAbstractResourceSearcher* serv);

    virtual void pleaseStop();

    CLNetState& getNetState()
    {
        return m_netState;
    }

protected:
    QnResourceDiscoveryManager();
    virtual void run();
private:
    // returns new resources( not from pool) or updates some in resource pool
    QnResourceList findNewResources(bool& ip_finished);

    void check_if_accessible(QnResourceList& justfoundList, int threads);

    void resovle_conflicts(QnResourceList& device_list, const CLIPList& busy_list, bool& ip_finished);

    

private:

    ResourceSearcherList m_searchersList;
    QMutex m_searchersListMtx;

    CLNetState m_netState;
};

#endif //cl_asynch_device_sarcher_h_423
