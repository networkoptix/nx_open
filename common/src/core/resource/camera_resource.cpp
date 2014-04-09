#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <utils/math/math.h>
#include <api/app_server_connection.h>

static const float MAX_EPS = 0.01f;
static const int MAX_ISSUE_CNT = 3; // max camera issues during a 1 min.
static const qint64 ISSUE_KEEP_TIMEOUT = 1000000ll * 60;

QnVirtualCameraResource::QnVirtualCameraResource():
    m_dtsFactory(0)
{}

QnPhysicalCameraResource::QnPhysicalCameraResource(): 
    QnVirtualCameraResource(),
    m_channelNumber(0)
{
    setFlags(local_live_cam);
}

int QnPhysicalCameraResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*10;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(192,result);
}

int QnPhysicalCameraResource::getChannel() const
{
    QMutexLocker lock(&m_mutex);
    return m_channelNumber;
}

void QnPhysicalCameraResource::setUrl(const QString &url)
{
    QnVirtualCameraResource::setUrl(url); /* This call emits, so we should not invoke it under lock. */

    QMutexLocker lock(&m_mutex);
    m_channelNumber = QUrlQuery(QUrl(url).query()).queryItemValue(QLatin1String("channel")).toInt();
    if (m_channelNumber > 0)
        m_channelNumber--; // convert human readable channel in range [1..x] to range [0..x-1]
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

        if (aspectRatio != 0 && !qBetween(qMin(ar1,ar2), aspectRatio, qMax(ar1,ar2)))
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

CameraDiagnostics::Result QnPhysicalCameraResource::initInternal() {
    return CameraDiagnostics::NoErrorResult();
}

// --------------- QnVirtualCameraResource ----------------------

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


QString QnVirtualCameraResource::getUniqueId() const
{
    return getPhysicalId();
    /*
    if (hasFlags(foreigner))
        return getPhysicalId() + getParentId().toString();
    else 
        return getPhysicalId();
    */
}

void QnVirtualCameraResource::save()
{
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    ec2::ErrorCode rez = conn->getCameraManager()->addCameraSync(::toSharedPointer(this));
    if (rez != ec2::ErrorCode::ok) {
        qCritical() << Q_FUNC_INFO << ": can't save resource params to Enterprise Controller. Resource physicalId: "
            << getPhysicalId() << ". Description: " << ec2::toString(rez);
    }
}

QString QnVirtualCameraResource::toSearchString() const
{
    QString result;
    QTextStream(&result) << QnNetworkResource::toSearchString() << " " << getModel() << " " << getFirmware() << " " << getVendor(); //TODO: #Elric evil!
    return result;
}


void QnVirtualCameraResource::issueOccured()
{
    QMutexLocker lock(&m_mutex);
    m_issueTimes.push_back(getUsecTimer());
    if (m_issueTimes.size() >= MAX_ISSUE_CNT) {
        if (!hasStatusFlags(HasIssuesFlag)) {
            addStatusFlags(HasIssuesFlag);
            lock.unlock();
            saveAsync();
        }
    }
}

int QnVirtualCameraResource::saveAsync()
{
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    return conn->getCameraManager()->addCamera(::toSharedPointer(this), this, &QnVirtualCameraResource::at_saveAsyncFinished);
}

void QnVirtualCameraResource::at_saveAsyncFinished(int, ec2::ErrorCode, const QnVirtualCameraResourceList &)
{
    // not used
}

void QnVirtualCameraResource::noCameraIssues()
{
    QMutexLocker lock(&m_mutex);
    qint64 threshold = getUsecTimer() - ISSUE_KEEP_TIMEOUT;
    while(!m_issueTimes.empty() && m_issueTimes.front() < threshold)
        m_issueTimes.pop_front();

    if (m_issueTimes.empty() && hasStatusFlags(HasIssuesFlag)) {
        removeStatusFlags(HasIssuesFlag);
        lock.unlock();
        saveAsync();
    }
}
