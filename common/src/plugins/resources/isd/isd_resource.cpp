#include "isd_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"


const char* QnPlIsdResource::MANUFACTURE = "ISD";

static QStringList getValues(const QString& line)
{
    QStringList result;

    int index = line.indexOf("=");

    if (index < 0)
        return result;

    QString values = line.mid(index+1);

    return values.split(",");
}

static bool sizeCompare(const QSize &s1, const QSize &s2)
{
    return s1.width() > s2.width();
}


//==================================================================


QnPlIsdResource::QnPlIsdResource()
{
    setAuth("root", "admin");
    
}

bool QnPlIsdResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlIsdResource::updateMACAddress()
{
    return true;
}

QString QnPlIsdResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

bool QnPlIsdResource::initInternal()
{
    return false;

    /*
    CLHttpStatus status;
    QByteArray reslst = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.ResolutionList",  getHostAddress(), 80, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }

    QString line(reslst);
    QStringList vals = getValues(line);


    QList<QSize> resolutions;

    foreach(const QString& val,  vals)
    {
        QStringList wh_s = val.split("x");
        if (wh_s.size()<2)
            continue;

        resolutions.push_back( QSize(wh_s.at(0).toInt(), wh_s.at(1).toInt()) );
    }

    if (resolutions.size() < 1 )
        return false;


    qSort(resolutions.begin(), resolutions.end(), sizeCompare);

    {
        QMutexLocker lock(&m_mutex);
        m_resolution1 = resolutions[0];
        m_resolution2 = QSize(0,0);


        double requestSquare = m_resolution1.width() * m_resolution1.height();

        if (requestSquare < MAX_EPS || requestSquare > maxResolutionSquare) return EMPTY_RESOLUTION_PAIR;

        int bestIndex = -1;
        double bestMatchCoeff = maxResolutionSquare > MAX_EPS ? (maxResolutionSquare / requestSquare) : INT_MAX;

        for (int i = 0; i < m_resolutionList.size(); ++i) {
            ResolutionPair tmp;

            tmp.first = qPower2Ceil(static_cast<unsigned int>(m_resolutionList[i].first + 1), 8);
            tmp.second = qPower2Floor(static_cast<unsigned int>(m_resolutionList[i].second - 1), 8);
            float ar1 = getResolutionAspectRatio(tmp);

            tmp.first = qPower2Floor(static_cast<unsigned int>(m_resolutionList[i].first - 1), 8);
            tmp.second = qPower2Ceil(static_cast<unsigned int>(m_resolutionList[i].second + 1), 8);
            float ar2 = getResolutionAspectRatio(tmp);

            if (!qBetween(aspectRatio, qMin(ar1,ar2), qMax(ar1,ar2)))
            {
                continue;
            }

            double square = m_resolutionList[i].first * m_resolutionList[i].second;
            if (square < MAX_EPS) continue;

            double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
            if (matchCoeff <= bestMatchCoeff + MAX_EPS) {
                bestIndex = i;
                bestMatchCoeff = matchCoeff;
            }
        }

    }
    
    
    
       /**/


    /*
    QByteArray reslst2 = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.ResolutionList",  getHostAddress(), 80, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }
    /**/

    /*
    QByteArray fpses = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.FrameRateList",  getHostAddress(), 80, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }

    /**/
    



    return true;

}

QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{

    return new QnRtpStreamReader(toSharedPointer(), "stream1");
}

void QnPlIsdResource::setCropingPhysical(QRect /*croping*/)
{
}

const QnResourceAudioLayout* QnPlIsdResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnRtpStreamReader* rtspReader = dynamic_cast<const QnRtpStreamReader*>(dataProvider);
        if (rtspReader && rtspReader->getDPAudioLayout())
            return rtspReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}
