/**********************************************************
* 1 oct 2014
* akolesnikov
***********************************************************/

#include "camera_user_attribute_pool.h"

#include <api/global_settings.h>


QnUserCameraSettings::QnUserCameraSettings()
:
    motionType(Qn::MT_Default),
    scheduleDisabled(true),
    audioEnabled(false),
    cameraControlDisabled( !QnGlobalSettings::instance()->isCameraSettingsOptimizationEnabled() )
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        motionRegions << QnMotionRegion();
}



////////////////////////////////////////////////////////////
//// QnCameraUserAttributePool
////////////////////////////////////////////////////////////

static QnCameraUserAttributePool* QnCameraUserAttributePool_instance = nullptr;

QnCameraUserAttributePool::QnCameraUserAttributePool()
{
    QnCameraUserAttributePool_instance = this;
}

QnCameraUserAttributePool::~QnCameraUserAttributePool()
{
    assert( QnCameraUserAttributePool_instance == this );
    QnCameraUserAttributePool_instance = nullptr;
}

QnCameraUserAttributePool* QnCameraUserAttributePool::instance()
{
    return QnCameraUserAttributePool_instance;
}
