#include "camera.h"

#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>

namespace nx {
namespace mediaserver {
namespace resource {

const float Camera::kMaxEps = 0.01f;

Camera::Camera(QnCommonModule* commonModule):
    QnVirtualCameraResource(commonModule),
    m_channelNumber(0)
{
    setFlags(Qn::local_live_cam);
    m_lastInitTime.invalidate();
}

int Camera::getChannel() const
{
    QnMutexLocker lock( &m_mutex );
    return m_channelNumber;
}

void Camera::setUrl(const QString &urlStr)
{
    QnVirtualCameraResource::setUrl( urlStr ); /* This call emits, so we should not invoke it under lock. */

    QnMutexLocker lock( &m_mutex );
    QUrl url( urlStr );
    m_channelNumber = QUrlQuery( url.query() ).queryItemValue( QLatin1String( "channel" ) ).toInt();
    setHttpPort( url.port( httpPort() ) );
    if (m_channelNumber > 0)
        m_channelNumber--; // convert human readable channel in range [1..x] to range [0..x-1]
}

float Camera::getResolutionAspectRatio(const QSize& resolution)
{
    if (resolution.height() == 0)
        return 0;
    float result = static_cast<double>(resolution.width()) / resolution.height();
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    if (resolution.width() == 720 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(4.0 / 3.0);
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    else if (resolution.width() == 960 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(16.0 / 9.0);
    return result;
}

QSize Camera::getNearestResolution(
    const QSize& resolution,
    float aspectRatio,
    double maxResolutionArea,
    const QList<QSize>& resolutionList,
    double* coeff)
{
    if (coeff)
        *coeff = INT_MAX;

    double requestSquare = resolution.width() * resolution.height();
    if (requestSquare < kMaxEps || requestSquare > maxResolutionArea) return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionArea > kMaxEps ? (maxResolutionArea / requestSquare) : INT_MAX;

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
        if (square < kMaxEps) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + kMaxEps) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
            if (coeff)
                *coeff = bestMatchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

QSize Camera::closestResolution(
    const QSize& idealResolution,
    float aspectRatio,
    const QSize& maxResolution,
    const QList<QSize>& resolutionList,
    double* outCoefficient)
{
    const auto maxResolutionArea = maxResolution.width() * maxResolution.height();

    QSize result = getNearestResolution(
        idealResolution,
        aspectRatio,
        maxResolutionArea,
        resolutionList,
        outCoefficient);

    if (result == EMPTY_RESOLUTION_PAIR)
    {
        result = getNearestResolution(
            idealResolution,
            0.0,
            maxResolutionArea,
            resolutionList,
            outCoefficient); //< Try to get resolution ignoring aspect ration
    }

    return result;
}

CameraDiagnostics::Result Camera::initInternal()
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    int timeoutSec = resData.value<int>(Qn::kUnauthrizedTimeoutParamName);
    auto credentials = getAuth();
    auto status = getStatus();
    if (timeoutSec > 0 &&
        m_lastInitTime.isValid() &&
        m_lastInitTime.elapsed() < timeoutSec * 1000 &&
        status == Qn::Unauthorized &&
        m_lastCredentials == credentials)
    {
        return CameraDiagnostics::NotAuthorisedResult(getUrl());
    }
    m_lastInitTime.restart();
    m_lastCredentials = credentials;
    return CameraDiagnostics::NoErrorResult();
}

} // namespace resource
} // namespace mediaserver
} // namespace nx
