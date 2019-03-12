#include "media_server_resource_searchers.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <nx/utils/app_info.h>

#include <camera_vendors.h>

#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>
#include <plugins/resource/desktop_camera/desktop_camera_deleter.h>
#include <plugins/resource/test_camera/testcamera_resource_searcher.h>

#include <plugins/resource/acti/acti_resource_searcher.h>
#include <plugins/resource/adam/adam_resource_searcher.h>
#include <plugins/resource/flir/flir_onvif_resource_searcher.h>
#include <plugins/resource/flir/flir_fc_resource_searcher.h>
#include <plugins/resource/arecontvision/resource/av_resource_searcher.h>
#include <plugins/resource/axis/axis_resource_searcher.h>
#include <plugins/resource/d-link/dlink_resource_searcher.h>
#include <plugins/resource/flex_watch/flexwatch_resource_searcher.h>
#include <plugins/resource/flir/flir_resource_searcher.h>
#include <plugins/resource/c2p_camera/c2p_camera_resource_searcher.h>

#if defined(ENABLE_IQINVISION)
    #include <plugins/resource/iqinvision/iqinvision_resource_searcher.h>
#endif
#include <plugins/resource/isd/isd_resource_searcher.h>
#include <plugins/resource/onvif/onvif_resource_searcher.h>
#include <plugins/resource/third_party/third_party_resource_searcher.h>
#include <plugins/resource/wearable/wearable_camera_resource_searcher.h>
#include <plugins/resource/archive_camera/archive_camera.h>

#if defined(ENABLE_HANWHA)
    #include <plugins/resource/hanwha/hanwha_resource_searcher.h>
#endif

#include <plugins/storage/dts/vmax480/vmax480_resource_searcher.h>
#include <common/common_module.h>
#include "media_server_module.h"

using namespace nx::plugins;

template <typename T>
void QnMediaServerResourceSearchers::registerSearcher(T* instance)
{
    m_searchers.insert(std::type_index(typeid(T)), instance);
    serverModule()->commonModule()->resourceDiscoveryManager()->addDeviceServer(instance);
}

QnMediaServerResourceSearchers::QnMediaServerResourceSearchers(QnMediaServerModule* serverModule):
    QObject(),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

void QnMediaServerResourceSearchers::initialize()
{

    auto commonModule = serverModule()->commonModule();

    //NOTE plugins have higher priority than built-in drivers
    registerSearcher(new ThirdPartyResourceSearcher(serverModule()));

    registerSearcher(new QnPlC2pCameraResourceSearcher(serverModule()));

#ifdef ENABLE_DESKTOP_CAMERA
    registerSearcher(new QnDesktopCameraResourceSearcher(serverModule()));

    // TODO: #GDM it this the best place for this class instance? Why not place it directly to the searcher?
    QnDesktopCameraDeleter* autoDeleter = new QnDesktopCameraDeleter(commonModule);
    Q_UNUSED(autoDeleter); /* Class instance will be auto-deleted in our dtor. */
#endif  //ENABLE_DESKTOP_CAMERA
#ifdef ENABLE_WEARABLE
    registerSearcher(new QnWearableCameraResourceSearcher(serverModule()));
#endif

#ifndef EDGE_SERVER
    #ifdef ENABLE_ARECONT
        registerSearcher(new QnPlArecontResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_DLINK
        registerSearcher(new QnPlDlinkResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_TEST_CAMERA
        registerSearcher(new QnTestCameraResourceSearcher((serverModule())));
    #endif
    #ifdef ENABLE_AXIS
        registerSearcher(new QnPlAxisResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_ACTI
        registerSearcher(new QnActiResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_IQINVISION
        registerSearcher(new QnPlIqResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_ISD
        registerSearcher(new QnPlISDResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_HANWHA
        registerSearcher(new nx::vms::server::plugins::HanwhaResourceSearcher(serverModule()));
    #endif

    #ifdef ENABLE_ADVANTECH
        registerSearcher(new QnAdamResourceSearcher(serverModule()));
    #endif
    #ifdef ENABLE_FLIR
        registerSearcher(new flir::FcResourceSearcher(serverModule()));
        registerSearcher(new QnFlirResourceSearcher(serverModule()));
        #ifdef ENABLE_ONVIF
        bool enableSequentialFlirOnvifSearcher = commonModule
            ->globalSettings()
            ->sequentialFlirOnvifSearcherEnabled();

        if (enableSequentialFlirOnvifSearcher)
            registerSearcher(new flir::OnvifResourceSearcher(serverModule()));
        #endif
    #endif
    #if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        registerSearcher(new QnPlVmax480ResourceSearcher(serverModule()));
    #endif

        registerSearcher(new QnArchiveCamResourceSearcher(serverModule()));

        //Onvif searcher should be the last:
    #ifdef ENABLE_ONVIF
        registerSearcher(new QnFlexWatchResourceSearcher(serverModule()));
        registerSearcher(new OnvifResourceSearcher(serverModule()));
    #endif //ENABLE_ONVIF
#endif
}

QnMediaServerResourceSearchers::~QnMediaServerResourceSearchers()
{
    clear();
}

void QnMediaServerResourceSearchers::clear()
{
    for (auto searcher: m_searchers.values())
        delete searcher;
    m_searchers.clear();
    // TODO: Investigate if cleaning QnResourceDiscoveryManager::m_searchersList is needed.
}
