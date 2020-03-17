#ifdef ENABLE_AXIS

#include "axis_stream_reader.h"

#include <set>

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>

#include "axis_resource.h"
#include "axis_resource.h"
#include <utils/media/av_codec_helper.h>

#include <nx/utils/app_info.h>

static const char AXIS_SEI_UUID[] = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";
static const int AXIS_SEI_PRODUCT_INFO = 0x0a00;
static const int AXIS_SEI_TIMESTAMP = 0x0a01;
static const int AXIS_SEI_TRIGGER_DATA = 0x0a03;

QnAxisStreamReader::QnAxisStreamReader(
    const QnPlAxisResourcePtr& res)
    :
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res, res->getTimeOffset()),
    m_axisRes(res)
{
}

QnAxisStreamReader::~QnAxisStreamReader()
{
    stop();
}

int QnAxisStreamReader::toAxisQuality(Qn::StreamQuality quality)
{
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
            return 50;
        case Qn::StreamQuality::low:
            return 50;
        case Qn::StreamQuality::normal:
            return 40;
        case Qn::StreamQuality::high:
            return 30;
        case Qn::StreamQuality::highest:
            return 22; //Axis selection for the best quality is "20" so we set 22 to save some resources for the secondary stream.
        case Qn::StreamQuality::preset:
            return -1;
        default:
            return -1;
    }
}

CameraDiagnostics::Result QnAxisStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    NX_VERBOSE(this, lit("try to get stream URL for camera %1 for role %2").arg(m_resource->getUrl()).arg(getRole()));

    if (isStreamOpened())
    {
        NX_VERBOSE(this, 
            "Stream is already opened for camera %1 for role %2", m_resource->getUrl(), getRole());
        return CameraDiagnostics::NoErrorResult();
    }

    //setRole(Qn::CR_SecondaryLiveVideo);

    // ==== init if needed
    Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);

    int channels = m_axisRes->getMaxChannels();

    QByteArray profileNumber("S");
    QByteArray profileName;
    QByteArray profileDescription;
    QByteArray action = "add";

    QString profileSufix;
    if (channels > 1)
    {
        // multiple channel encoder
        profileSufix = QString::number(m_axisRes->getChannel() + 1);
    }

    if (m_axisRes->hasVideo(this))
    {
        static const QByteArray OLD_PRIMARY_STREAM_PROFILE_NAME = "netOptixPrimary";
        static const QByteArray OLD_SECONDARY_STREAM_PROFILE_NAME = "netOptixSecondary";

        QByteArray oldProfileName;
        const auto brand = nx::utils::AppInfo::brand();
        if (role == Qn::CR_LiveVideo)
        {
            profileName = lit("%1Primary%2").arg(brand).arg(profileSufix).toUtf8();
            profileDescription = QString(lit("%1 Primary Stream")).arg(brand).toUtf8();
            oldProfileName = OLD_PRIMARY_STREAM_PROFILE_NAME;
        }
        else
        {
            profileName = lit("%1Secondary%2").arg(brand).arg(profileSufix).toUtf8();
            profileDescription = QString(lit("%1 Secondary Stream")).arg(brand).toUtf8();
            oldProfileName = OLD_SECONDARY_STREAM_PROFILE_NAME;
        }

        // ================ profile setup ========================

        // -------------- check if profile already exists

        std::set<QByteArray> profilesToRemove;
        for (int i = 0; i < 3; ++i)
        {
            CLSimpleHTTPClient http(m_axisRes->getHostAddress(), QUrl(m_axisRes->getUrl()).port(80), m_axisRes->getNetworkTimeout(), m_axisRes->getAuth());
            const QString& requestPath = QLatin1String("/axis-cgi/param.cgi?action=list&group=StreamProfile");
            CLHttpStatus status = http.doGET(requestPath);

            if (status != CL_HTTP_SUCCESS)
            {
                NX_DEBUG(this, 
                    "Can not get stream URL for camera %1 for role %2. Request failed with code %3", 
                    m_resource->getUrl(), getRole(), status);

                if (status == CL_HTTP_AUTH_REQUIRED)
                {
                    return CameraDiagnostics::NotAuthorisedResult(m_axisRes->getUrl());
                }
                else if (status == CL_HTTP_NOT_FOUND && !m_oldFirmwareWarned)
                {
                    NX_ERROR(this, lit("Axis camera must be have old firmware!!!!  ip = %1").arg(m_axisRes->getHostAddress()));
                    m_oldFirmwareWarned = true;
                    return CameraDiagnostics::RequestFailedResult(requestPath, QLatin1String("old firmware"));
                }

                return CameraDiagnostics::RequestFailedResult(requestPath, QLatin1String(nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)status)));
            }

            QByteArray body;
            http.readAll(body);
            if (body.isEmpty())
            {
                msleep(nx::utils::random::number(0, 50));
                continue; // sometime axis returns empty profiles list
            }

            QList<QByteArray> lines = body.split('\n');
            bool profileFound = false;
            for (int i = 0; i < lines.size(); ++i)
            {
                if (lines[i].endsWith(QByteArray("Name=") + profileName))
                {
                    action = "update"; // profile already exists, so update instead of add
                    QList<QByteArray> params = lines[i].split('.');
                    if (params.size() >= 2)
                    {
                        QByteArray token = params[params.size()-2];
                        if (!profileFound)
                        {
                            action = "update"; // profile already exists, so update instead of add
                            profileNumber = token;
                        }
                        else
                        {
                            profilesToRemove.insert( token );
                        }
                        profileFound = true;
                    }

                }
                else if (lines[i].endsWith(QByteArray("Name=") + oldProfileName))
                {
                    //removing this profile
                    auto tokens = lines[i].split('.');
                    if (tokens.size() > 3)
                        profilesToRemove.insert(tokens[2]);
                }
            }
        }

        //removing old profiles
        for (const auto& profileToRemove : profilesToRemove)
        {
            CLSimpleHTTPClient http(m_axisRes->getHostAddress(), QUrl(m_axisRes->getUrl()).port(80), m_axisRes->getNetworkTimeout(), m_axisRes->getAuth());
            const QString& requestPath = QLatin1String("/axis-cgi/param.cgi?action=remove&group=root.StreamProfile." + profileToRemove);
            const int httpStatus = http.doGET(requestPath);    //ignoring error code
            if (httpStatus != CL_HTTP_SUCCESS)
            {
                NX_DEBUG(this, lit("Failed to remove old Axis profile %1 on camera %2").
                    arg(QLatin1String(profileToRemove)).arg(m_axisRes->getHostAddress()));
            }
        }

        // ------------------- determine stream parameters ----------------------------
        if (params.resolution.isEmpty())
            qWarning() << "Can't determine max resolution for axis camera " << m_axisRes->getName() << "use default resolution";
        Qn::StreamQuality quality = params.quality;

        QByteArray paramsStr;
        paramsStr.append(lit("videocodec=%1").arg(m_axisRes->toAxisCodecString(
            QnAvCodecHelper::codecIdFromString(params.codec))));
        if (!params.resolution.isEmpty())
            paramsStr.append("&resolution=").append(m_axisRes->resolutionToString(params.resolution));
        //paramsStr.append("&text=0"); // do not use onscreen text message (fps e.t.c)
        paramsStr.append("&fps=").append(QByteArray::number(params.fps));
        if (params.bitrateKbps > 0)
            paramsStr.append("&videobitrate=").append(QByteArray::number(params.bitrateKbps));
        else if (quality != Qn::StreamQuality::preset)
            paramsStr.append("&compression=").append(QByteArray::number(toAxisQuality(quality)));
        paramsStr.append("&audio=").append(m_axisRes->isAudioEnabled() ? "1" : "0");

        // --------------- update or insert new profile ----------------------

        if (action == QByteArray("add") || isCameraControlRequired)
        {
            QString streamProfile;
            QTextStream str(&streamProfile);

            str << "/axis-cgi/param.cgi?action=" << action;
            str << "&template=streamprofile";
            if (action != "update")
                str << "&group=StreamProfile";
            str << "&StreamProfile." << profileNumber << ".Name=" << profileName;
            str << "&StreamProfile." << profileNumber << ".Description=" << QUrl::toPercentEncoding(QLatin1String(profileDescription));
            str << "&StreamProfile." << profileNumber << ".Parameters=" << QUrl::toPercentEncoding(QLatin1String(paramsStr));
            str.flush();

            CLSimpleHTTPClient http(m_axisRes->getHostAddress(), QUrl(m_axisRes->getUrl()).port(80), m_axisRes->getNetworkTimeout(), m_axisRes->getAuth());
            CLHttpStatus status = http.doGET(streamProfile);

            if (status == CL_HTTP_AUTH_REQUIRED)
            {
                return CameraDiagnostics::NotAuthorisedResult(m_axisRes->getUrl());
            }
            else if (status != CL_HTTP_SUCCESS)
            {
                return CameraDiagnostics::RequestFailedResult(CameraDiagnostics::RequestFailedResult(streamProfile, QLatin1String(nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)status))));
            }

            if (role != Qn::CR_SecondaryLiveVideo && m_axisRes->getMotionType() != Qn::MotionType::MT_SoftwareGrid)
            {
                m_axisRes->setMotionMaskPhysical(0);
            }
        }
    }

    QUrlQuery query;
    if (!profileName.isEmpty())
        query.addQueryItem("streamprofile", profileName);
    if (channels > 1)
        query.addQueryItem("camera", QString::number( m_axisRes->getChannel() + 1));

    QUrl streamUrl(m_resource->getUrl());
    streamUrl.setScheme("rtsp");
    streamUrl.setPort(m_axisRes->rtspPort());
    streamUrl.setPath("/axis-media/media.amp");
    streamUrl.setQuery(query);

    NX_INFO(this, lit("got stream URL %1 for camera %2 for role %3").arg(streamUrl.toString()).arg(m_resource->getUrl()).arg(getRole()));

    // ============== requesting a video ==========================
    m_rtpStreamParser.setRequest(streamUrl.toString());
    m_axisRes->updateSourceUrl(m_rtpStreamParser.getCurrentStreamUrl(), getRole());
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
    QnMetaDataV1Ptr rez = m_lastMetadata != 0
        ? m_lastMetadata
        : QnMetaDataV1Ptr(new QnMetaDataV1(qnSyncTime->currentTimePoint()));
    m_lastMetadata.reset();
    if (rez)
        filterMotionByMask(rez);
    return rez;
}

void QnAxisStreamReader::fillMotionInfo(const QRect& rect)
{
    if (m_lastMetadata == 0)
    {
        m_lastMetadata = QnMetaDataV1Ptr(new QnMetaDataV1(qnSyncTime->currentTimePoint()));
        m_lastMetadata->m_duration = 1000 * 1000 * 10; // 10 sec
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
    QList<QByteArray> params = QByteArray((const char*)payload, len).split(';');
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

void QnAxisStreamReader::parseMotionInfo(QnCompressedVideoData* videoData)
{
    const quint8* curNal = (const quint8*)videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);
    //int prefixSize = 3;
    //if (end - curNal >= 4 && curNal[2] == 0)
    //    prefixSize = 4;

    const quint8* nextNal = curNal;
    for (; curNal < end; curNal = nextNal)
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
                    switch (msgType)
                    {
                        case AXIS_SEI_PRODUCT_INFO:
                            break;
                        case AXIS_SEI_TIMESTAMP:
                            break;
                        case AXIS_SEI_TRIGGER_DATA:
                            processTriggerData(payload + 4, msgLen - 4);
                            break;
                        default:
                            break;
                    }
                    payload += msgLen + 2;
                    len -= msgLen + 2;
                }
            }
        }
    }
}

bool QnAxisStreamReader::isGotFrame(QnCompressedVideoDataPtr videoData)
{
    const quint8* curNal = (const quint8*)videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);

    const quint8* nextNal = curNal;
    for (; curNal < end; curNal = nextNal)
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
    if (getRole() == Qn::CR_LiveVideo && m_axisRes->getMotionType() != Qn::MotionType::MT_SoftwareGrid)
        m_axisRes->readMotionInfo();

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez)
        {
            if (rez->dataType == QnAbstractMediaData::VIDEO)
            {
                parseMotionInfo(static_cast<QnCompressedVideoData*>(rez.get()));
                //if (isGotFrame(videoData))
                break;
            }
            else if (rez->dataType == QnAbstractMediaData::AUDIO)
            {
                break;
            }
        }
        else
        {
            closeStream();
            break;
        }
    }

    return rez;
}

QnConstResourceAudioLayoutPtr QnAxisStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

#endif // #ifdef ENABLE_AXIS
