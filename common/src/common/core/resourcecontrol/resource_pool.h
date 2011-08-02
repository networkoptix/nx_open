#ifndef resource_pool_h_1537
#define resource_pool_h_1537
#include "resourcecontrol/asynch_seacher.h"
#include "resource/resource.h"
#include "resource_criteria.h"
#include "resource/id.h"

class QnResource;
class QnNetworkResource;
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
    
    void addResource(QnResourcePtr resource);
    void removeResource(QnResourcePtr resource);

    QnResourceList getResources() const;

    QnResourcePtr getResourceById(QString id) const;

    // if it has; returns the pointer to the first equal
    QnResourcePtr hasEqualResource(QnResourcePtr res) const;

    // returns list of resources with such flag
    QnResourceList getResourcesWithFlag(unsigned long flag);


private:
	

private:
    mutable QMutex m_resourcesMtx;
    typedef QMap<QnId, QnResourcePtr> ResourceMap;
	ResourceMap m_resources;

};

#endif //resource_pool_h_1537
