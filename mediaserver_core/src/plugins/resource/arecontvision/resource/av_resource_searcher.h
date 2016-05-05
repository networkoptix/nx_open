#ifndef av_device_server_h_2107
#define av_device_server_h_2107

#ifdef ENABLE_ARECONT

#include "core/resource_management/resource_searcher.h"
#include "plugins/resource/arecontvision/resource/av_resource.h"

#include <array>

class QnPlArecontResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    typedef std::array<unsigned char, 6> MacArray;

public:
    QnPlArecontResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:
    QnNetworkResourcePtr findResourceHelper(const MacArray &mac, 
                                            const SocketAddress &addr);
};

#endif

#endif // av_device_server_h_2107
