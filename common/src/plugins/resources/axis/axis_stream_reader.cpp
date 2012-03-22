#include "axis_resource.h"
#include "axis_stream_reader.h"
#include <QTextStream>
#include "axis_resource.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"


QnAxisStreamReader::QnAxisStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_RTP264(res)
{

}

QnAxisStreamReader::~QnAxisStreamReader()
{

}

int QnAxisStreamReader::toAxisQuality(QnStreamQuality quality)
{
    switch(quality)
    {
        case QnQualityLowest:
            return 60;
        case QnQualityLow:
            return 50;
        case QnQualityNormal:
            return 30;
        case QnQualityHigh:
            return 20;
        case QnQualityHighest:
            return 15;
        case QnQualityPreSeted:
            return -1;
    }
    return -1;
}

void QnAxisStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    //setRole(QnResource::Role_SecondaryLiveVideo);

    //==== init if needed
    QnResource::ConnectionRole role = getRole();
    QnPlAxisResourcePtr res = getResource().dynamicCast<QnPlAxisResource>();

    QByteArray profileName;
    QByteArray profileDescription;
    QByteArray action = "add";
    if (role == QnResource::Role_LiveVideo)
    {
        profileName = "netOptixPrimary";
        profileDescription = "Network Optix Primary Stream";
    }
    else {
        profileName = "netOptixSecondary";
        profileDescription = "Network Optix Secondary Stream";
    }


    //================ profile setup ======================== 
    
    // -------------- check if profile already exists
    {
        QString streamProfile;
        QTextStream str(&streamProfile);

        str << "/axis-cgi/param.cgi?action=list";
        str << "&group=StreamProfile";
        str.flush();

        CLSimpleHTTPClient http (res->getHostAddress(), QUrl(res->getUrl()).port(80), res->getNetworkTimeout(), res->getAuth());
        CLHttpStatus status = http.doGET(streamProfile);
        QByteArray body;
        http.readAll(body);
        if (body.contains(QByteArray("Name=") +profileName))
            action = "update"; // profile already exists, so update instead of add
    }
    // ------------------- determine stream parameters ----------------------------
    float fps = getFps();
    float ar = res->getResolutionAspectRatio(res->getMaxResolution());
    QString resolution = (role == QnResource::Role_LiveVideo) ? res->getMaxResolution() : res->getNearestResolution("320x240", ar);
    if (resolution.isEmpty()) 
        qWarning() << "Can't determine max resolution for axis camera " << res->getName() << "use default resolution";
    QnStreamQuality quality = getQuality();

    QByteArray paramsStr;
    paramsStr.append("videocodec=h264");
    if (!resolution.isEmpty())
        paramsStr.append("&resolution=").append(resolution);
    //paramsStr.append("&text=0"); // do not use onscreen text message (fps e.t.c)
    paramsStr.append("&fps=").append(QByteArray::number(fps));
    if (quality != QnQualityPreSeted)
        paramsStr.append("&compression=").append(QByteArray::number(toAxisQuality(quality)));

    // --------------- update or insert new profile ----------------------
    
    QString streamProfile;
    QTextStream str(&streamProfile);

    str << "/axis-cgi/param.cgi?action=" << action;
    str << "&template=streamprofile";
    str << "&group=StreamProfile";
    str << "&StreamProfile.S.Name=" << profileName;
    str << "&StreamProfile.S.Description=" << QUrl::toPercentEncoding(profileDescription);
    str << "&StreamProfile.S.Parameters=" << QUrl::toPercentEncoding(paramsStr);
    str.flush();

    CLSimpleHTTPClient http (res->getHostAddress(), QUrl(res->getUrl()).port(80), res->getNetworkTimeout(), res->getAuth());
    CLHttpStatus status = http.doGET(streamProfile);

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        getResource()->setStatus(QnResource::Unauthorized);
        return;
    }
    else if (status != CL_HTTP_SUCCESS)
        return;


    // ============== requesting a video ==========================
    m_RTP264.setRequest(QString("axis-media/media.amp?streamprofile=") + profileName);
    m_RTP264.openStream();
}

void QnAxisStreamReader::closeStream()
{
    m_RTP264.closeStream();
}

bool QnAxisStreamReader::isStreamOpened() const
{
    return m_RTP264.isStreamOpened();
}

QnMetaDataV1Ptr QnAxisStreamReader::getMetaData()
{
    // todo: implement me
    QnMetaDataV1Ptr rez(new QnMetaDataV1());
    return rez;
}

QnAbstractMediaDataPtr QnAxisStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();


    QnAbstractMediaDataPtr rez = m_RTP264.getNextData();
    if (!rez) 
        closeStream();
    
    return rez;
}

void QnAxisStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnAxisStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

//=====================================================================================

