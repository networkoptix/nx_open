#include "isd_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"
#include "utils/common/math.h"
#include "api/app_server_connection.h"
#include "isd_stream_reader.h"


const char* QnPlIsdResource::MANUFACTURE = "ISD";
QString QnPlIsdResource::MAX_FPS_PARAM_NAME = QString("MaxFPS");

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

static float getResolutionAspectRatio(QSize s)
{
    if (s.height()==0)
        return 0;

    return float(s.width()) / s.height();
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


        double maxResolutionSquare = m_resolution1.width() * m_resolution1.height();
        double requestSquare = 320 * 240;



        int bestIndex = -1;
        double bestMatchCoeff = maxResolutionSquare / requestSquare;

        float bestAspectRatio = getResolutionAspectRatio(m_resolution1);

        for (int i = 0; i < resolutions.size(); ++i) 
        {
            QSize tmp;

            tmp.setWidth( qPower2Ceil(static_cast<unsigned int>(resolutions[i].width()+ 1), 8) );
            tmp.setHeight( qPower2Floor(static_cast<unsigned int>(resolutions[i].height() - 1), 8) );
            float ar1 = getResolutionAspectRatio(tmp);

            tmp.setWidth( qPower2Floor(static_cast<unsigned int>(resolutions[i].width() - 1), 8) );
            tmp.setHeight( qPower2Ceil(static_cast<unsigned int>(resolutions[i].height() + 1), 8) );
            float ar2 = getResolutionAspectRatio(tmp);

            if (!qBetween(bestAspectRatio, qMin(ar1,ar2), qMax(ar1,ar2)))
            {
                continue;
            }

            double square = resolutions[i].width() * resolutions[i].height();
            

            double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);

            if (matchCoeff <= bestMatchCoeff + 0.002) 
            {
                bestIndex = i;
                bestMatchCoeff = matchCoeff;
            }
        }

        if (resolutions[bestIndex].width() * resolutions[bestIndex].height() < 640*480)
            m_resolution2 = resolutions[bestIndex];

    }
    
    
    
       /**/


    /*
    QByteArray reslst2 = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.2.ResolutionList",  getHostAddress(), 80, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }
    /**/


    QByteArray fpses = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.FrameRateList",  getHostAddress(), 80, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }

    /**/
    

    QString line2(fpses);
    vals = getValues(line2);

    QList<int> fpsList;
    foreach(QString fpsS, vals)
    {
        fpsList.push_back(fpsS.trimmed().toInt());
    }

    qSort(fpsList.begin(),fpsList.end(), qGreater<int>());

    if (fpsList.size()<1)
        return false;

    {
        
        setMaxFps(fpsList.at(0));
    }

    save();

    return true;

}

QSize QnPlIsdResource::getPrimaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolution1;

}

QSize QnPlIsdResource::getSecondaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolution2;
}


QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{

    return new QnISDStreamReader(toSharedPointer());
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


void QnPlIsdResource::setMaxFps(int f)
{
    setParam(MAX_FPS_PARAM_NAME, f, QnDomainDatabase);
}

int QnPlIsdResource::getMaxFps()
{
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlIsdResource*>(this);

    if (!hasSuchParam(MAX_FPS_PARAM_NAME))
    {
        return QnPhysicalCameraResource::getMaxFps();
    }

    this_casted->getParam(MAX_FPS_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toInt();
}

void QnPlIsdResource::save()
{
    QByteArray errorStr;
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>(), errorStr) != 0) {
        qCritical() << "QnPlOnvifResource::init: can't save resource params to Enterprise Controller. Resource physicalId: "
            << getPhysicalId() << ". Description: " << errorStr;
    }
}
