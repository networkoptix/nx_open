#include "camera_resource.h"
#include "resource_consumer.h"
#include "common/common_meta_types.h"

QnVirtualCameraResource::QnVirtualCameraResource()
    : m_scheduleDisabled(true),
      m_audioEnabled(false),
      m_manuallyAdded(false),
      m_advancedWorking(false),
      m_dtsFactory(0)
{
    QnCommonMetaTypes::initilize();
}

QnPhysicalCameraResource::QnPhysicalCameraResource(): QnVirtualCameraResource()
{
    setFlags(local_live_cam);
}

int QnPhysicalCameraResource::suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const
{
    // I assume for a QnQualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a QnQualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*10;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;

    int result = lowEnd + (hiEnd - lowEnd) * (q - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(128,result);
}

void QnVirtualCameraResource::updateInner(QnResourcePtr other)
{
    QnNetworkResource::updateInner(other);
    QnSecurityCamResource::updateInner(other);

    QnVirtualCameraResourcePtr camera = other.dynamicCast<QnVirtualCameraResource>();
    if (camera)
    {
        m_scheduleDisabled = camera->isScheduleDisabled();
        m_audioEnabled = camera->isAudioEnabled();
        m_manuallyAdded = camera->isManuallyAdded();
    }
}

void QnVirtualCameraResource::setScheduleDisabled(bool disabled)
{
    QMutexLocker locker(&m_mutex);

    if(m_scheduleDisabled == disabled)
        return;

    m_scheduleDisabled = disabled;

    locker.unlock();
    emit scheduleDisabledChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isScheduleDisabled() const
{
    return m_scheduleDisabled;
}

void QnVirtualCameraResource::setAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
}

bool QnVirtualCameraResource::isAudioEnabled() const
{
    return m_audioEnabled;
}

bool QnVirtualCameraResource::isManuallyAdded() const
{
    return m_manuallyAdded;
}
void QnVirtualCameraResource::setManuallyAdded(bool value)
{
    m_manuallyAdded = value;
}


bool QnVirtualCameraResource::isAdvancedWorking() const
{
    return m_advancedWorking;
}

void QnVirtualCameraResource::setAdvancedWorking(bool value)
{
    m_advancedWorking = value;
}

QnAbstractDTSFactory* QnVirtualCameraResource::getDTSFactory()
{
    return m_dtsFactory;
}

void QnVirtualCameraResource::setDTSFactory(QnAbstractDTSFactory* factory)
{
    m_dtsFactory = factory;
}

void QnVirtualCameraResource::lockDTSFactory()
{
    m_mutex.lock();
}

void QnVirtualCameraResource::unLockDTSFactory()
{
    m_mutex.unlock();
}

QString QnVirtualCameraResource::getModel() const
{
    return m_model;
}

void QnVirtualCameraResource::setModel(QString model)
{
    QMutexLocker locker(&m_mutex);
    m_model = model;
}

QString QnVirtualCameraResource::getFirmware() const
{
    QMutexLocker locker(&m_mutex);
    return m_firmware;
}

void QnVirtualCameraResource::setFirmware(QString firmware)
{
    QMutexLocker locker(&m_mutex);
    m_firmware = firmware;
}
