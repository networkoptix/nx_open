#ifndef dw_dwr_device_server_h_2219
#define dw_dwr_device_server_h_2219

#include "core/resourcemanagment/resource_searcher.h"

class DwDvrResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    DwDvrResourceSearcher();
    virtual ~DwDvrResourceSearcher();
public:
    static DwDvrResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;
protected:
    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
    virtual QnResourceList findResources() override;
    
    void getCamerasFromDvr(QnResourceList& resources, const QString& host, int port, const QString& login, const QString& password);
};

#endif // dw_dwr_device_server_h_2219
