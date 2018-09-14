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

QnMediaServerResourceSearchers::QnMediaServerResourceSearchers(QnMediaServerModule* serverModule):
    QObject(),
    nx::mediaserver::ServerModuleAware(serverModule)
{
    auto commonModule = serverModule->commonModule();

    //NOTE plugins have higher priority than built-in drivers
    m_searchers << new ThirdPartyResourceSearcher(serverModule);

    m_searchers << new QnPlC2pCameraResourceSearcher(commonModule);

#ifdef ENABLE_DESKTOP_CAMERA
    m_searchers << new QnDesktopCameraResourceSearcher(serverModule);

    // TODO: #GDM it this the best place for this class instance? Why not place it directly to the searcher?
    QnDesktopCameraDeleter* autoDeleter = new QnDesktopCameraDeleter(commonModule);
    Q_UNUSED(autoDeleter); /* Class instance will be auto-deleted in our dtor. */
#endif  //ENABLE_DESKTOP_CAMERA
#ifdef ENABLE_WEARABLE
    m_searchers << new QnWearableCameraResourceSearcher(serverModule);
#endif

#ifndef EDGE_SERVER
    #ifdef ENABLE_ARECONT
        m_searchers << new QnPlArecontResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_DLINK
        m_searchers << new QnPlDlinkResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_TEST_CAMERA
        m_searchers << new QnTestCameraResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_AXIS
        m_searchers << new QnPlAxisResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_ACTI
        m_searchers << new QnActiResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_IQINVISION
        m_searchers << new QnPlIqResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_ISD
        m_searchers << new QnPlISDResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_HANWHA
        m_searchers << new nx::mediaserver_core::plugins::HanwhaResourceSearcher(serverModule);
    #endif

    #ifdef ENABLE_ADVANTECH
        m_searchers << new QnAdamResourceSearcher(serverModule);
    #endif
    #ifdef ENABLE_FLIR
        m_flirIoExecutor = std::make_unique<nx::plugins::flir::IoExecutor>();

        m_searchers << new flir::FcResourceSearcher(serverModule);
        m_searchers << new QnFlirResourceSearcher(serverModule);
        #ifdef ENABLE_ONVIF
        bool enableSequentialFlirOnvifSearcher = commonModule
            ->globalSettings()
            ->sequentialFlirOnvifSearcherEnabled();

        if (enableSequentialFlirOnvifSearcher)
            m_searchers << new flir::OnvifResourceSearcher(serverModule);
        #endif
    #endif
    #if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        m_searchers << new QnPlVmax480ResourceSearcher(serverModule);
    #endif

        m_searchers << new QnArchiveCamResourceSearcher(serverModule);

        //Onvif searcher should be the last:
    #ifdef ENABLE_ONVIF
        m_searchers << new QnFlexWatchResourceSearcher(serverModule);
        m_searchers << new OnvifResourceSearcher(serverModule);
    #endif //ENABLE_ONVIF
#endif

    for (auto searcher: m_searchers)
        commonModule->resourceDiscoveryManager()->addDeviceServer(searcher);
}

QnMediaServerResourceSearchers::~QnMediaServerResourceSearchers()
{
    for (auto searcher : m_searchers)
        delete searcher;
}
