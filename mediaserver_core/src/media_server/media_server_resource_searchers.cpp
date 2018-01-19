#include "media_server_resource_searchers.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_discovery_manager.h>

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
#include <plugins/resource/iqinvision/iqinvision_resource_searcher.h>
#include <plugins/resource/isd/isd_resource_searcher.h>
#include <plugins/resource/hanwha/hanwha_resource_searcher.h>
#include <plugins/resource/onvif/onvif_resource_searcher.h>
#include <plugins/resource/stardot/stardot_resource_searcher.h>
#include <plugins/resource/third_party/third_party_resource_searcher.h>
#include <plugins/resource/wearable/wearable_camera_resource_searcher.h>
#include <plugins/resource/archive_camera/archive_camera.h>

#include <plugins/storage/dts/vmax480/vmax480_resource_searcher.h>
#include <common/common_module.h>

using namespace nx::plugins;

QnMediaServerResourceSearchers::QnMediaServerResourceSearchers(QnCommonModule* commonModule):
    QObject(),
    QnCommonModuleAware(commonModule)
{
    //NOTE plugins have higher priority than built-in drivers
    m_searchers << new ThirdPartyResourceSearcher(commonModule);

#ifdef ENABLE_DESKTOP_CAMERA
    m_searchers << new QnDesktopCameraResourceSearcher(commonModule);

    // TODO: #GDM it this the best place for this class instance? Why not place it directly to the searcher?
    QnDesktopCameraDeleter* autoDeleter = new QnDesktopCameraDeleter(this);
    Q_UNUSED(autoDeleter); /* Class instance will be auto-deleted in our dtor. */
#endif  //ENABLE_DESKTOP_CAMERA
#ifdef ENABLE_WEARABLE
    m_searchers << new QnWearableCameraResourceSearcher(commonModule);
#endif

#ifndef EDGE_SERVER
    #ifdef ENABLE_ARECONT
        m_searchers << new QnPlArecontResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_DLINK
        m_searchers << new QnPlDlinkResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_TEST_CAMERA
        m_searchers << new QnTestCameraResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_AXIS
        m_searchers << new QnPlAxisResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_ACTI
        m_searchers << new QnActiResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_STARDOT
        m_searchers << new QnStardotResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_IQE
        m_searchers << new QnPlIqResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_ISD
        m_searchers << new QnPlISDResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_HANWHA
        m_searchers << new nx::mediaserver_core::plugins::HanwhaResourceSearcher(commonModule);
    #endif

    #ifdef ENABLE_ADVANTECH
        m_searchers << new QnAdamResourceSearcher(commonModule);
    #endif
    #ifdef ENABLE_FLIR
        m_searchers << new flir::FcResourceSearcher(commonModule);
        m_searchers << new QnFlirResourceSearcher(commonModule);
        #ifdef ENABLE_ONVIF
        bool enableSequentialFlirOnvifSearcher = QnCommonModuleAware::commonModule()
            ->globalSettings()
            ->sequentialFlirOnvifSearcherEnabled();

        if (enableSequentialFlirOnvifSearcher)
            m_searchers << new flir::OnvifResourceSearcher(commonModule);
        #endif
    #endif
    #if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        m_searchers << new QnPlVmax480ResourceSearcher(commonModule);
    #endif

        m_searchers << new QnArchiveCamResourceSearcher(commonModule);

        //Onvif searcher should be the last:
    #ifdef ENABLE_ONVIF
        m_searchers << new QnFlexWatchResourceSearcher(commonModule);
        m_searchers << new OnvifResourceSearcher(commonModule);
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
