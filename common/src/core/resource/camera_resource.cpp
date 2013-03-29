#include "camera_resource.h"
#include "resource_consumer.h"
#include "api/app_server_connection.h"

static const float MAX_EPS = 0.01f;

QReadWriteLock QnVirtualCameraResource::m_stopAppLock;
bool QnVirtualCameraResource::m_pleaseStop = false;

QnVirtualCameraResource::QnVirtualCameraResource():
    m_scheduleDisabled(true),
    m_audioEnabled(false),
    m_manuallyAdded(false),
    m_advancedWorking(false),
    m_dtsFactory(0)
{}

QnPhysicalCameraResource::QnPhysicalCameraResource(): 
    QnVirtualCameraResource(),
    m_channelNumer(0)
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
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(128,result);
}

int QnPhysicalCameraResource::getChannel() const
{
    QMutexLocker lock(&m_mutex);
    return m_channelNumer;
}

void QnPhysicalCameraResource::setUrl(const QString &url)
{
    QUrl u(url);

    QMutexLocker lock(&m_mutex);
    QnVirtualCameraResource::setUrl(url);
    m_channelNumer = u.queryItemValue(QLatin1String("channel")).toInt();
    if (m_channelNumer > 0)
        m_channelNumer--; // convert human readable channel in range [1..x] to range [0..x]
}

float QnPhysicalCameraResource::getResolutionAspectRatio(const QSize& resolution)
{
    if (resolution.height() == 0)
        return 0;
    float result = static_cast<double>(resolution.width()) / resolution.height();
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    if (resolution.width() == 720 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(4.0 / 3.0);
    return result;
}

QSize QnPhysicalCameraResource::getNearestResolution(const QSize& resolution, float aspectRatio,
                                              double maxResolutionSquare, const QList<QSize>& resolutionList)
{
    double requestSquare = resolution.width() * resolution.height();
    if (requestSquare < MAX_EPS || requestSquare > maxResolutionSquare) return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionSquare > MAX_EPS ? (maxResolutionSquare / requestSquare) : INT_MAX;

    for (int i = 0; i < resolutionList.size(); ++i) {
        QSize tmp;

        tmp.setWidth(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].width() + 1), 8));
        tmp.setHeight(qPower2Floor(static_cast<unsigned int>(resolutionList[i].height() - 1), 8));
        float ar1 = getResolutionAspectRatio(tmp);

        tmp.setWidth(qPower2Floor(static_cast<unsigned int>(resolutionList[i].width() - 1), 8));
        tmp.setHeight(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].height() + 1), 8));
        float ar2 = getResolutionAspectRatio(tmp);

        if (aspectRatio != 0 && !qBetween(aspectRatio, qMin(ar1,ar2), qMax(ar1,ar2)))
        {
            continue;
        }

        double square = resolutionList[i].width() * resolutionList[i].height();
        if (square < MAX_EPS) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + MAX_EPS) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

// --------------- QnVirtualCameraResource ----------------------

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
        m_model = camera->m_model;
        m_firmware = camera->m_firmware;
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
    QMutexLocker locker(&m_mutex);
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

QString QnVirtualCameraResource::getUniqueId() const
{
	if (hasFlags(foreigner))
		return getPhysicalId() + getParentId().toString();
	else 
		return getPhysicalId();

}

void QnVirtualCameraResource::deserialize(const QnResourceParameters &parameters) {
    QnNetworkResource::deserialize(parameters);

    if (!isDtsBased() && supportedMotionType() != Qn::MT_NoMotion)
        addFlags(motion);
}

void QnVirtualCameraResource::onStopApplication()
{
    QWriteLocker lock(&m_stopAppLock);
    m_pleaseStop = true;
}

void QnVirtualCameraResource::save()
{
    QReadLocker lock(&m_stopAppLock);
    if (m_pleaseStop)
        return;

    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>()) != 0) {
        qCritical() << "QnPlOnvifResource::init: can't save resource params to Enterprise Controller. Resource physicalId: "
            << getPhysicalId() << ". Description: " << conn->getLastError();
    }
}

