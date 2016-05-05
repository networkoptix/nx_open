#include "media_server_resource_searchers.h"

#include <core/resource_management/resource_discovery_manager.h>

#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>
#include <plugins/resource/desktop_camera/desktop_camera_deleter.h>
#include <plugins/resource/test_camera/testcamera_resource_searcher.h>

#include <plugins/resource/acti/acti_resource_searcher.h>
#include <plugins/resource/arecontvision/resource/av_resource_searcher.h>
#include <plugins/resource/axis/axis_resource_searcher.h>
#include <plugins/resource/d-link/dlink_resource_searcher.h>
#include <plugins/resource/flex_watch/flexwatch_resource_searcher.h>
#include <plugins/resource/iqinvision/iqinvision_resource_searcher.h>
#include <plugins/resource/isd/isd_resource_searcher.h>
#include <plugins/resource/onvif/onvif_resource_searcher.h>
#include <plugins/resource/stardot/stardot_resource_searcher.h>
#include <plugins/resource/third_party/third_party_resource_searcher.h>
#include <plugins/resource/archive_camera/archive_camera.h>

#include <plugins/storage/dts/vmax480/vmax480_resource_searcher.h>

QnMediaServerResourceSearchers::QnMediaServerResourceSearchers(QObject* parent /*= nullptr*/) :
    QObject(parent)
{
    //NOTE plugins have higher priority than built-in drivers
    m_searchers << new ThirdPartyResourceSearcher();

#ifdef ENABLE_DESKTOP_CAMERA
    m_searchers << new QnDesktopCameraResourceSearcher();

    //TODO: #GDM it this the best place for this class instance? Why not place it directly to the searcher?
    QnDesktopCameraDeleter* autoDeleter = new QnDesktopCameraDeleter(this);
    Q_UNUSED(autoDeleter); /* Class instance will be auto-deleted in our dtor. */
#endif  //ENABLE_DESKTOP_CAMERA

#ifndef EDGE_SERVER
    #ifdef ENABLE_ARECONT
        m_searchers << new QnPlArecontResourceSearcher();
    #endif
    #ifdef ENABLE_DLINK
        m_searchers << new QnPlDlinkResourceSearcher();
    #endif
    #ifdef ENABLE_TEST_CAMERA
        m_searchers << new QnTestCameraResourceSearcher();
    #endif
    #ifdef ENABLE_AXIS
        m_searchers << new QnPlAxisResourceSearcher();
    #endif
    #ifdef ENABLE_ACTI
        m_searchers << new QnActiResourceSearcher();
    #endif
    #ifdef ENABLE_STARDOT
        m_searchers << new QnStardotResourceSearcher();
    #endif
    #ifdef ENABLE_IQE
        m_searchers << new QnPlIqResourceSearcher();
    #endif
    #ifdef ENABLE_ISD
        m_searchers << new QnPlISDResourceSearcher();
    #endif

    #if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        m_searchers << new QnPlVmax480ResourceSearcher();
    #endif

        m_searchers << new QnArchiveCamResourceSearcher();

        //Onvif searcher should be the last:
    #ifdef ENABLE_ONVIF
        m_searchers << new QnFlexWatchResourceSearcher();
        m_searchers << new OnvifResourceSearcher();
    #endif //ENABLE_ONVIF
#endif

    for (auto searcher : m_searchers)
        QnResourceDiscoveryManager::instance()->addDeviceServer(searcher);
}

QnMediaServerResourceSearchers::~QnMediaServerResourceSearchers()
{
    for (auto searcher : m_searchers)
        delete searcher;
}
