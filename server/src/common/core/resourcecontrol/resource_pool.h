#ifndef resource_pool_h_1537
#define resource_pool_h_1537
#include "resourcecontrol/asynch_seacher.h"
#include "resource/resource.h"
#include "resource_criteria.h"

class QnResource;
class CLNetworkDevice;
class CLRecorderDevice;

// this class holds all resources in the system ( as long as resource is in the pool => share pointer counter >= 1)
// so if resource is in the pool it guaranties it cannot be deleted
// in addition to that it also can give list of resources based on some criteria
// and helps to administrate resources 

class QnResourcePool 
{
    QnResourcePool();
	~QnResourcePool();
public:
	static QnResourcePool& instance();
    
    void addResource(QnResource* resource);
    void removeResource(QnResource* resource);

    QnResource* getResourceById(QString id) const;


private:
	

private:
    mutable QMutex m_resourcesMtx;
	QnResourceList m_resources;
    


};

#endif //resource_pool_h_1537
