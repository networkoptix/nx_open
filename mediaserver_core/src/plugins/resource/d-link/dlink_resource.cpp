#ifdef ENABLE_DLINK

#include "dlink_resource.h"

#include <nx/network/deprecated/asynchttpclient.h>

#include "dlink_stream_reader.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"

#include <motion/motion_detection.h>

const QString QnPlDlinkResource::MANUFACTURE(lit("Dlink"));

namespace
{

    bool sizeCompare(const QSize &s1, const QSize &s2)
    {
        return s1.width() > s2.width();
    }

    bool profileCompare(const QnDlink_ProfileInfo &left, const QnDlink_ProfileInfo &right)
    {
        bool isH264Left = (left.codec == "H264");
        bool isH264Right = (right.codec == "H264");
        if (isH264Left != isH264Right)
            return isH264Left > isH264Right;
        return left.number < right.number;
    }

    int extractProfileNum(const QByteArray& key)
    {
        QByteArray result;
        int rightPos = key.size()-1;
        while (rightPos >= 0 && key.at(rightPos) >= '0' && key.at(rightPos) <= '9')
            --rightPos;
        return key.mid(rightPos+1).toInt();
    }

    bool hasLiteralContinuation(const QByteArray& key, const QByteArray&prefix)
    {
        QByteArray suffix = key.mid(prefix.size());
        for (int i = 0; i < suffix.size(); ++i) {
            bool isDigit = suffix.at(i) >= '0' && suffix.at(i) <= '9';
            if (!isDigit)
                return true;
        }
        return false;
    }
}


QnDlink_cam_info::QnDlink_cam_info()
{
}

void QnDlink_cam_info::clear()
{

    resolutions.clear();
    possibleBitrates.clear();
    possibleFps.clear();

    possibleQualities.clear();
    profiles.clear();
}

// returns resolution with width not less than width
QSize QnDlink_cam_info::resolutionCloseTo(int width) const
{
    if (resolutions.size()==0)
    {
        NX_ASSERT(false);
        return QSize(0,0);
    }

    QSize result = resolutions.at(0);


    for(const QSize& size: resolutions)
    {
        if (size.width() <= width)
            return size;
        else
            result = size;
    }

    return result;
}

// returns next up bitrate
QByteArray QnDlink_cam_info::bitrateCloseTo(int val)
{

    QSize result;

    if (possibleBitrates.size()==0)
    {
        NX_ASSERT(false);
        return QByteArray();
    }

    auto it = possibleBitrates.lowerBound(val);
    if (it == possibleBitrates.end())
        it--;

    return it.value();

}

// returns next up frame rate
int QnDlink_cam_info::frameRateCloseTo(int fr)
{
    NX_ASSERT(possibleFps.size()>0);

    int result = possibleFps.at(0);

    for(int fps: possibleFps)
    {
        if (fps <= fr)
        {
            // can return fps or result
            int dif1 = abs(fps-fr);
            int dif2 = abs(result-fr);

            return dif1 <= dif2 ? fps : result;
        }
        else
            result = fps;
    }

    return result;
}

QSize QnDlink_cam_info::primaryStreamResolution() const
{
    return resolutions.at(0);
}

QSize QnDlink_cam_info::secondaryStreamResolution() const
{
    return resolutionCloseTo(480);
}


// =======================================================================================

QnPlDlinkResource::QnPlDlinkResource()
{
    setVendor(lit("Dlink"));
}

void QnPlDlinkResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    QAuthenticator auth = getAuth();

    nx::utils::Url apiUrl;
    apiUrl.setScheme( lit("http") );
    apiUrl.setHost( getHostAddress() );
    apiUrl.setPort( QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT) );
    apiUrl.setUserName( auth.user() );
    apiUrl.setPassword( auth.password() );
    apiUrl.setPath( lit("/common/info.cgi") );

    QString resourceMac = getMAC().toString();
    auto requestCompletionFunc = [resourceMac, completionHandler]
        ( SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody ) mutable
    {
        if( osErrorCode != SystemError::noError ||
            statusCode != nx::network::http::StatusCode::ok )
        {
            return completionHandler( false );
        }

        //msgBody contains parameters "param1=value1" each on its line

        nx::network::http::LineSplitter lineSplitter;
        QnByteArrayConstRef line;
        size_t bytesRead = 0;
        size_t dataOffset = 0;
        while( lineSplitter.parseByLines( nx::network::http::ConstBufferRefType(msgBody, dataOffset), &line, &bytesRead) )
        {
            dataOffset += bytesRead;
            const int sepIndex = line.indexOf('=');
            if( sepIndex == -1 )
                continue;
            if( line.mid( 0, sepIndex ) == "macaddr" )
            {
                QByteArray mac = line.mid( sepIndex+1 );
                mac.replace( ':', '-' );
                return completionHandler( mac == resourceMac.toLatin1() );
            }
        }

        completionHandler( false );
    };

    nx::network::http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc );
}

QString QnPlDlinkResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlDlinkResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlDlinkResource::createLiveDataProvider()
{
    //return new MJPEGStreamReader(toSharedPointer(), "ipcam/stream.cgi?nowprofileid=2&audiostream=0");
    //return new MJPEGStreamReader(toSharedPointer(), "video/mjpg.cgi");
    //return new MJPEGStreamReader(toSharedPointer(), "video/mjpg.cgi?profileid=2");
    return new PlDlinkStreamReader(toSharedPointer());
}

void QnPlDlinkResource::setCroppingPhysical(QRect /*cropping*/)
{

}

QnDlink_cam_info QnPlDlinkResource::getCamInfo() const
{
    QnMutexLocker mutexLocker( &m_mutex );
    return m_camInfo;
}

nx::mediaserver::resource::StreamCapabilityMap QnPlDlinkResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnPlDlinkResource::initializeCameraDriver()
{
    updateDefaultAuthIfEmpty(QLatin1String("admin"), QLatin1String(""));

    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, QLatin1String("config/stream_info.cgi"),  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
        return CameraDiagnostics::UnknownErrorResult();
    }


    if (cam_info_file.size()==0)
        return CameraDiagnostics::UnknownErrorResult();


    QList<QByteArray> lines = cam_info_file.split('\n');

    m_camInfo.clear();

    QMap<int, QnDlink_ProfileInfo> profilesMap;

    for(QByteArray line: lines)
    {
        line = line.trimmed();
        int splitterPos = line.indexOf(L'=');
        if (splitterPos == -1)
            continue;
        const QByteArray key = line.left(splitterPos).trimmed();
        const QByteArray value = line.mid(splitterPos+1).trimmed();

        if (key == "videos")
        {
            ; // nothing to do
        }
        else if (key == "resolutions")
        {
            for(const QByteArray& val:  value.split(','))
            {
                QList<QByteArray> wh_s = val.split('x');
                if (wh_s.size()<2)
                    continue;

                m_camInfo.resolutions.push_back( QSize(wh_s.at(0).toInt(), wh_s.at(1).toInt()) );
            }

        }
        else if (key == "framerates")
        {
            QList<QByteArray> vals = value.split(',');
            for(const QByteArray& val:  vals)
                m_camInfo.possibleFps.push_back( val.toInt() );
        }
        else if (key == "vbitrates")
        {
            for(const QByteArray& bs: value.split(','))
            {
                bool m = bs.toLower().contains('m');
                bool k = bs.toLower().contains('k');

                QByteArray t = bs;
                if (m || k)
                    t = t.left(t.length()-1);

                int val = t.toInt();
                if(m)
                    val *= 1024;

                m_camInfo.possibleBitrates[val] = bs;
            }
        }
        else if (key == "vprofilenum") {
            ; // nothing to do
        }
        else if (key == "qualities") {
            m_camInfo.possibleQualities = value.split(',');
        }
        else if (key.startsWith("vprofileurl")) {
            profilesMap[extractProfileNum(key)].url = QLatin1String(value);
        }
        else if (key.startsWith("vprofile") && !hasLiteralContinuation(key, "vprofile")) {
            profilesMap[extractProfileNum(key)].codec = value.toUpper();
        }
    }


    std::sort(m_camInfo.possibleFps.begin(), m_camInfo.possibleFps.end(), std::greater<int>());
    std::sort(m_camInfo.resolutions.begin(), m_camInfo.resolutions.end(), sizeCompare);

    for (auto itr = profilesMap.begin(); itr != profilesMap.end(); ++itr) {
        itr.value().number = itr.key();
        m_camInfo.profiles << itr.value();
    }
    std::sort(m_camInfo.profiles.begin(), m_camInfo.profiles.end(), profileCompare);

    // =======remove elements with diff aspect ratio
    if (m_camInfo.resolutions.size() < 2)
        return CameraDiagnostics::UnknownErrorResult();


    int w_0 = m_camInfo.resolutions.at(0).width();
    int h_0 = m_camInfo.resolutions.at(0).height();
    float apectRatio_0 = ((float)w_0/h_0);

    int w_1 = m_camInfo.resolutions.at(1).width();
    int h_1 = m_camInfo.resolutions.at(1).height();
    float apectRatio_1 = ((float)w_1/h_1);

    float apectRatio = apectRatio_0;

    if (std::abs(apectRatio_0 - apectRatio_1) > 0.01)
        apectRatio = apectRatio_1; //

    QList<QSize>::iterator it = m_camInfo.resolutions.begin();

    while (it != m_camInfo.resolutions.end())
    {
        QSize s = *it;

        float apectRatioL = (float)s.width() / s.height();

        if ( qAbs(apectRatioL - apectRatio) > 0.01 )
            it = m_camInfo.resolutions.erase(it);
        else
            ++it;
    }

    saveParams();

    return CameraDiagnostics::NoErrorResult();

}

static void setBitAt(int x, int y, unsigned char* data)
{
    NX_ASSERT(x<32);
    NX_ASSERT(y<16);

    int offset = (y * 32 + x) / 8;
    data[offset] |= 0x01 << (x&7);
}


void QnPlDlinkResource::setMotionMaskPhysical(int channel)
{
    Q_UNUSED(channel);

    if (channel != 0)
        return; // motion info used always once even for multisensor cameras

    static int sensToLevelThreshold[10] =
    {
        0, // 0 - aka mask really filtered by server always
        10, // 1
        25, // 2
        40, // 3
        50, // 4
        60, // 5
        70,  // 6
        80,  // 7
        90,  // 8
        100   // 9
    };

    int sensitivity = 50;
    QnMotionRegion region = getMotionRegion(0);
    for (int sens = 1; sens < QnMotionRegion::kSensitivityLevelCount; ++sens)
    {

        if (!region.getRegionBySens(sens).isEmpty())
        {
            sensitivity = sensToLevelThreshold[sens];
            break; // only 1 sensitivity for all frame is supported
        }
    }

    unsigned char maskBit[Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8];
    QnMetaDataV1::createMask(getMotionMask(0),  (char*)maskBit);


    QImage img(Qn::kMotionGridWidth, Qn::kMotionGridHeight, QImage::Format_Mono);
    memset(img.bits(), 0, img.byteCount());
    img.setColor(0, qRgb(0, 0, 0));
    img.setColor(1, qRgb(255, 255, 255));


    for (int x = 0; x  < Qn::kMotionGridWidth; ++x)
    {
        for (int y = 0; y  < Qn::kMotionGridHeight; ++y)
        {
            if (QnMetaDataV1::isMotionAt(x,y,(char*)maskBit))
                img.setPixel(x,y,1);
        }
    }



    QImage imgOut = img.scaled(32, 16);//, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    //imgOut.save("c:/img.bmp");

    unsigned char outData[32*16/8];
    memset(outData,0,sizeof(outData));

    for (int x = 0; x  < 32; ++x)
    {
        for (int y = 0; y  < 16; ++y)
        {
            if (imgOut.pixel(x,y) == img.color(0))
                setBitAt(x,y, outData);
        }
    }



    QString str;
    QTextStream stream(&str);
    stream << "config/motion.cgi?enable=yes&motioncvalue=" << sensitivity << "&mbmask=";

    for (uint i = 0; i < sizeof(outData); ++i)
    {
        QString t = QString::number(outData[i], 16).toUpper();
        if (t.length() < 2)
            t = QLatin1String("0") + t;
        stream << t[1] << t[0];
    }

    stream.flush();


    CLHttpStatus status;
    QByteArray result = downloadFile(status, str,  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
        return;
    }

}

#endif
