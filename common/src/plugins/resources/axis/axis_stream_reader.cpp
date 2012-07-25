#include <QTextStream>
#include "axis_resource.h"
#include "axis_stream_reader.h"
#include "axis_resource.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"

static const char AXIS_SEI_UUID[] = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";
static const int AXIS_SEI_PRODUCT_INFO = 0x0a00;
static const int AXIS_SEI_TIMESTAMP = 0x0a01;
static const int AXIS_SEI_TRIGGER_DATA = 0x0a03;


QnAxisStreamReader::QnAxisStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    QnLiveStreamProvider(res),
    m_rtpStreamParser(res),
    m_oldFirmwareWarned(false)
{
    m_axisRes = getResource().dynamicCast<QnPlAxisResource>();
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
        case QnQualityPreSet:
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

    int channels = 1;
    if (res->hasSuchParam("channelsAmount"))
    {
        QVariant val;
        res->getParam("channelsAmount", val, QnDomainMemory);
        channels = val.toUInt();
    }


    QByteArray profileNumber("S");
    QByteArray profileName;
    QByteArray profileDescription;
    QByteArray action = "add";

    QByteArray profileSufix;
    if (channels > 1)
    {
        // multiple channel encoder
        profileSufix = QByteArray::number(res->getChannelNum());
    }

    if (role == QnResource::Role_LiveVideo)
    {
        profileName = "netOptixPrimary" + profileSufix;
        profileDescription = "Network Optix Primary Stream";
    }
    else {
        profileName = "netOptixSecondary" + profileSufix;
        profileDescription = "Network Optix Secondary Stream";
    }


    //================ profile setup ======================== 
    
    // -------------- check if profile already exists
    {
        CLSimpleHTTPClient http (res->getHostAddress(), QUrl(res->getUrl()).port(80), res->getNetworkTimeout(), res->getAuth());
        CLHttpStatus status = http.doGET(QByteArray("/axis-cgi/param.cgi?action=list&group=StreamProfile"));

        if (status != CL_HTTP_SUCCESS)
        {
            if (status == CL_HTTP_AUTH_REQUIRED)
            {
                getResource()->setStatus(QnResource::Unauthorized);
            }
            else if (status == CL_HTTP_NOT_FOUND && !m_oldFirmwareWarned) 
            {
                cl_log.log("Axis camera must be have old firmware!!!!  ip = ",  res->getHostAddress().toString() , cl_logERROR);
                m_oldFirmwareWarned = true;
            }

            return;
        }

        QByteArray body;
        http.readAll(body);
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
    if (quality != QnQualityPreSet)
        paramsStr.append("&compression=").append(QByteArray::number(toAxisQuality(quality)));
    paramsStr.append("&audio=").append(res->isAudioEnabled() ? "1" : "0");

    // --------------- update or insert new profile ----------------------
    
    QString streamProfile;
    QTextStream str(&streamProfile);

    str << "/axis-cgi/param.cgi?action=" << action;
    str << "&template=streamprofile";
    str << "&group=StreamProfile";
    str << "&StreamProfile." << profileNumber << ".Name=" << profileName;
    str << "&StreamProfile." << profileNumber << ".Description=" << QUrl::toPercentEncoding(profileDescription);
    str << "&StreamProfile." << profileNumber << ".Parameters=" << QUrl::toPercentEncoding(paramsStr);
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

    if (role != QnResource::Role_SecondaryLiveVideo && m_axisRes->getMotionType() != MT_SoftwareGrid)
    {
        res->setMotionMaskPhysical(0);
    }

    QString request;
    QTextStream stream(&request);
    stream << "axis-media/media.amp?streamprofile=" << profileName;


    if (channels > 1) //
    {
        qWarning() << "starting axis channel " << res->getChannelNum();
        stream << "&camera=" << res->getChannelNum();
    }

    

    // ============== requesting a video ==========================
    m_rtpStreamParser.setRequest(request);
    m_rtpStreamParser.openStream();
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
    const quint8* curNal = (const quint8*) videoData->data.data();
    const quint8* end = curNal + videoData->data.size();
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
    const quint8* curNal = (const quint8*) videoData->data.data();
    const quint8* end = curNal + videoData->data.size();
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
    CLLongRunnable::pleaseStop();
    m_rtpStreamParser.closeStream();
}

QnAbstractMediaDataPtr QnAxisStreamReader::getNextData()
{
    if (getRole() == QnResource::Role_LiveVideo) 
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

const QnResourceAudioLayout* QnAxisStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}
