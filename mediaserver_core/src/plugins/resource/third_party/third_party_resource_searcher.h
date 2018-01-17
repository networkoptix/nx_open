/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_RESOURCE_SEARCHER_H
#define THIRD_PARTY_RESOURCE_SEARCHER_H

#ifdef ENABLE_THIRD_PARTY

#include <list>

#include <QtCore/QList>
#include <QtCore/QVector>

#include <nx/network/upnp/upnp_search_handler.h>

#include "third_party_resource.h"
#include "../mdns/mdns_resource_searcher.h"
#include "plugins/resource/upnp/upnp_resource_searcher.h"
#include "plugins/camera_plugin.h"
#include "plugins/camera_plugin_qt_wrapper.h"


/*!
    \note One object is created for all loaded plugin
*/
class ThirdPartyResourceSearcher
:
    public QnMdnsResourceSearcher,
    public QnUpnpResourceSearcherAsync
{
public:
    /*!
        Adds ref to \a plugin
    */
    ThirdPartyResourceSearcher(QnCommonModule* commonModule);
    /*!
        Releases ref to \a plugin
    */
    virtual ~ThirdPartyResourceSearcher();

    //!Implementation of QnResourceFactory::createResource
    virtual QnResourcePtr createResource( const QnUuid &resourceTypeId, const QnResourceParams &params ) override;
    // return the manufacture of the server
    virtual QString manufacture() const override;
    //!Implementation of QnAbstractNetworkResourceSearcher::checkHostAddr
    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck ) override;

    static void initStaticInstance( ThirdPartyResourceSearcher* _instance );
    static ThirdPartyResourceSearcher* instance();

protected:
    //!Implementation of QnMdnsResourceSearcher::processPacket
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& /*result*/,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) override;
    //!Implementation of QnUpnpResourceSearcherAsync::processPacket
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const nx::network::SocketAddress& host,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) override;
    //!Implementation of QnAbstractResourceSearcher::findResources
    virtual QnResourceList findResources() override;

private:
    std::list<nxcip_qt::CameraDiscoveryManager> m_thirdPartyCamPlugins;

    //!Searchers resources using custom search method
    QnResourceList doCustomSearch();
    QnResourceList createResListFromCameraInfoList(
        nxcip_qt::CameraDiscoveryManager* const discoveryManager,
        const QVector<nxcip::CameraInfo>& cameraInfoArray );
    QnThirdPartyResourcePtr createResourceFromCameraInfo(
        nxcip_qt::CameraDiscoveryManager* const discoveryManager,
        const nxcip::CameraInfo& cameraInfo );
};

#endif // ENABLE_THIRD_PARTY

#endif  //THIRD_PARTY_RESOURCE_SEARCHER_H
