#ifdef ENABLE_AXIS

#include "axis_stream_reader.h"

#include <QtCore/QTextStream>

#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>

#include "axis_resource.h"
#include "axis_resource.h"
#include <utils/common/app_info.h>
static const char AXIS_SEI_UUID[] = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";
static const int AXIS_SEI_PRODUCT_INFO = 0x0a00;
static const int AXIS_SEI_TIMESTAMP = 0x0a01;
static const int AXIS_SEI_TRIGGER_DATA = 0x0a03;


QnAxisStreamReader::QnAxisStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res),
    m_oldFirmwareWarned(false)
{
    m_axisRes = getResource().dynamicCast<QnPlAxisResource>();
}

QnAxisStreamReader::~QnAxisStreamReader()
{
    stop();
}

int QnAxisStreamReader::toAxisQuality(Qn::StreamQuality quality)
{
    switch(quality)
    {
        case Qn::QualityLowest:
            return 50;
        case Qn::QualityLow:
            return 50;
        case Qn::QualityNormal:
            return 30;
        case Qn::QualityHigh:
            return 20;
        case Qn::QualityHighest:
            return 15;
        case Qn::QualityPreSet:
            return -1;
        default:
            return -1;
    }
}

CameraDiagnostics::Result QnAxisStreamReader::openStream()
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    //setRole(Qn::CR_SecondaryLiveVideo);

    //==== init if needed
    Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);
    QnPlAxisResourcePtr res = getResource().dynamicCast<QnPlAxisResource>();

    int channels = 1;
    if (res->hasParam(QLatin1String("channelsAmount")))
    {
        QVariant val;
        res->getParam(QLatin1String("channelsAmount"), val, QnDomainMemory);
        channels = val.toUInt();
    }


    QByteArray profileNumber("S");
    QByteArray profileName;
    QByteArray profileDescription;
    QByteArray action = "add";

    QString profileSufix;
    if (channels > 1)
    {
        // multiple channel encoder
        profileSufix = QString::number(res->getChannelNumAxis());
    }

    if (role == Qn::CR_LiveVideo)
    {
        profileName = lit("%1Primary%2").arg(QnAppInfo::productNameShort()).arg(profileSufix).toUtf8();
        profileDescription = QString(lit("%1 Primary Stream")).arg(QnAppInfo::productNameShort()).toUtf8();
    }
    else {
        profileName = lit("%1Secondary%2").arg(QnAppInfo::productNameShort()).arg(profileSufix).toUtf8();
        profileDescription = QString(lit("%1 Secondary Stream")).arg(QnAppInfo::productNameShort()).toUtf8();
    }


    //================ profile setup ======================== 
    
    // -------------- check if profile already exists
    for (int i = 0; i < 3; ++i)
    {
        CLSimpleHTTPClient http (res->getHostAddress(), QUrl(res->getUrl()).port(80), res->getNetworkTimeout(), res->getAuth());
        const QString& requestPath = QLatin1String("/axis-cgi/param.cgi?action=list&group=StreamProfile");
        CLHttpStatus status = http.doGET(requestPath);

        if (status != CL_HTTP_SUCCESS)
        {
            if (status == CL_HTTP_AUTH_REQUIRED)
            {
                getResource()->setStatus(Qn::Unauthorized);
                return CameraDiagnostics::NotAuthorisedResult( res->getUrl() );
            }
            else if (status == CL_HTTP_NOT_FOUND && !m_oldFirmwareWarned) 
            {
                NX_LOG( lit("Axis camera must be have old firmware!!!!  ip = %1").arg(res->getHostAddress()) , cl_logERROR);
                m_oldFirmwareWarned = true;
                return CameraDiagnostics::RequestFailedResult( requestPath, QLatin1String("old firmware") );
            }

            return CameraDiagnostics::RequestFailedResult(requestPath, QLatin1String(nx_http::StatusCode::toString((nx_http::StatusCode::Value)status)));
        }

        QByteArray body;
        http.readAll(body);
        if (body.isEmpty()) {
            msleep(rand() % 50);
            continue; // sometime axis returns empty profiles list
        }
        
        QList<QByteArray> lines = body.split('\n');
        for (int i = 0; i < lines.size(); ++i)
        {
            if (lines[i].endsWith(QByteArray("Name=") +profileName))
            {
                action = "update"; // profile already exists, so update instead of add
                QList<QByteArray> params = lines[i].split('.');
                if (params.size() >= 2)
                    profileNumber = params[params.size()-2];
            }
        }
    }
    // ------------------- determine stream parameters ----------------------------
    float fps = getFps();
    const QnPlAxisResource::AxisResolution& resolution = res->getResolution(
        role == Qn::CR_LiveVideo
            ? QnPlAxisResource::PRIMARY_ENCODER_INDEX
            : QnPlAxisResource::SECONDARY_ENCODER_INDEX );

    if (resolution.size.isEmpty()) 
        qWarning() << "Can't determine max resolution for axis camera " << res->getName() << "use default resolution";
    Qn::StreamQuality quality = getQuality();

    QByteArray paramsStr;
    paramsStr.append("videocodec=h264");
    if (!resolution.size.isEmpty())
        paramsStr.append("&resolution=").append(resolution.resolutionStr);
    //paramsStr.append("&text=0"); // do not use onscreen text message (fps e.t.c)
    paramsStr.append("&fps=").append(QByteArray::number(fps));
    if (quality != Qn::QualityPreSet)
        paramsStr.append("&compression=").append(QByteArray::number(toAxisQuality(quality)));
    paramsStr.append("&audio=").append(res->isAudioEnabled() ? "1" : "0");

    // --------------- update or insert new profile ----------------------
    
    if (action == QByteArray("add") || !isCameraControlDisabled())
    {
        QString streamProfile;
        QTextStream str(&streamProfile);

        str << "/axis-cgi/param.cgi?action=" << action;
        str << "&template=streamprofile";
        str << "&group=StreamProfile";
        str << "&StreamProfile." << profileNumber << ".Name=" << profileName;
        str << "&StreamProfile." << profileNumber << ".Description=" << QUrl::toPercentEncoding(QLatin1String(profileDescription));
        str << "&StreamProfile." << profileNumber << ".Parameters=" << QUrl::toPercentEncoding(QLatin1String(paramsStr));
        str.flush();

        CLSimpleHTTPClient http (res->getHostAddress(), QUrl(res->getUrl()).port(80), res->getNetworkTimeout(), res->getAuth());
        CLHttpStatus status = http.doGET(streamProfile);

        if (status == CL_HTTP_AUTH_REQUIRED)
        {
            getResource()->setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( res->getUrl() );
        }
        else if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::RequestFailedResult(CameraDiagnostics::RequestFailedResult(streamProfile, QLatin1String(nx_http::StatusCode::toString((nx_http::StatusCode::Value)status))));

        if (role != Qn::CR_SecondaryLiveVideo && m_axisRes->getMotionType() != Qn::MT_SoftwareGrid)
        {
            res->setMotionMaskPhysical(0);
        }
    }

    QString request;
    QTextStream stream(&request);
    //stream << "rtsp://" << QUrl(m_resource->getUrl()).host() << ":" << (QUrl(m_resource->getUrl()).port()+1) << "/"; // for port forwarding purpose
    stream << "axis-media/media.amp?streamprofile=" << profileName;


    if (channels > 1)
    {
        stream << "&camera=" << res->getChannelNumAxis();
    }

    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(request).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);
    

    // ============== requesting a video ==========================
    m_rtpStreamParser.setRequest(request);
    return m_rtpStreamParser.openStream();
}

void QnAxisStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool QnAxisStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr QnAxisStreamReader::getCameraMetadata()
{
    QnMetaDataV1Ptr rez = m_lastMetadata != 0 ? m_lastMetadata : QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastMetadata.clear();
    if (rez)
        filterMotionByMask(rez);
    return rez;
}

void QnAxisStreamReader::fillMotionInfo(const QRect& rect)
{
    if (m_lastMetadata == 0) {
        m_lastMetadata = QnMetaDataV1Ptr(new QnMetaDataV1());
        m_lastMetadata->m_duration = 1000*1000*10; // 10 sec 
    }
    for (int x = rect.left(); x <= rect.right(); ++x)
    {
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            m_lastMetadata->setMotionAt(x, y);
    }
}

void QnAxisStreamReader::processTriggerData(const quint8* payload, int len)
{
    if (len < 1)
        return;
    QList<QByteArray> params = QByteArray((const char*)payload,len).split(';');
    for (int i = 0; i < params.size(); ++i)
    {
        const QByteArray& param = params.at(i);
        if (param.length() > 2 && param[0] == 'M' && param[1] >= '0' && param[1] <= '9')
        {
            int motionWindowNum = param[1] - '0';
            bool isMotionExists = param.endsWith('1');
            if (isMotionExists)
                fillMotionInfo(m_axisRes->getMotionWindow(motionWindowNum));
        }
    }
}

void QnAxisStreamReader::parseMotionInfo(QnCompressedVideoDataPtr videoData)
{
    const quint8* curNal = (const quint8*) videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);
    //int prefixSize = 3;
    //if (end - curNal >= 4 && curNal[2] == 0)
    //    prefixSize = 4;

    const quint8* nextNal = curNal;
    for (;curNal < end; curNal = nextNal)
    {
        nextNal = NALUnit::findNextNAL(curNal, end);
        quint8 nalUnitType = *curNal & 0x1f;
        if (nalUnitType != nuSEI)
            continue;
        // parse SEI message
        SPSUnit sps;
        SEIUnit sei;
        sei.decodeBuffer(curNal, nextNal);
        sei.deserialize(sps, 0);
        for (int i = 0; i < sei.m_userDataPayload.size(); ++i)
        {
            const quint8* payload = sei.m_userDataPayload[i].first;
            int len = sei.m_userDataPayload[i].second;
            if (len >= 16 && strncmp((const char*)payload, AXIS_SEI_UUID, 16) == 0)
            {
                // AXIS SEI message detected
                payload += 16;
                len -= 16;
                while (len >= 4)
                {
                    int msgLen = (payload[0] << 8) + payload[1];
                    int msgType = (payload[2] << 8) + payload[3];
                    switch(msgType)
                    {
                    case AXIS_SEI_PRODUCT_INFO:
                        break;
                    case AXIS_SEI_TIMESTAMP:
                        break;
                    case AXIS_SEI_TRIGGER_DATA:
                        processTriggerData(payload+4, msgLen-4);
                        break;
                    default:
                        break;
                    }
                    payload += msgLen+2;
                    len -= msgLen+2;
                }
            }
        }
    }
}


bool QnAxisStreamReader::isGotFrame(QnCompressedVideoDataPtr videoData)
{
    const quint8* curNal = (const quint8*) videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);

    const quint8* nextNal = curNal;
    for (;curNal < end; curNal = nextNal)
    {
        nextNal = NALUnit::findNextNAL(curNal, end);
        quint8 nalUnitType = *curNal & 0x1f;
        if (nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
            return true;
    }
    return false;
}

void QnAxisStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_rtpStreamParser.pleaseStop();
}

QnAbstractMediaDataPtr QnAxisStreamReader::getNextData()
{
    if (getRole() == Qn::CR_LiveVideo && m_axisRes->getMotionType() != Qn::MT_SoftwareGrid) 
        m_axisRes->readMotionInfo();

    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData()) 
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez) 
        {
            if (rez->dataType == QnAbstractMediaData::VIDEO) {
                QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
                parseMotionInfo(videoData);
                //if (isGotFrame(videoData))
                break;
            }
            else if (rez->dataType == QnAbstractMediaData::AUDIO) {
                break;
            }
        }
        else {
            closeStream();
            break;
        }
    }
    
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

QnConstResourceAudioLayoutPtr QnAxisStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

#endif // #ifdef ENABLE_AXIS
