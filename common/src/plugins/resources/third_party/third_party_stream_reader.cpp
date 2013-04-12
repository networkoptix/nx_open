/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_stream_reader.h"

#include <algorithm>

#include <QTextStream>

#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"


ThirdPartyStreamReader::ThirdPartyStreamReader(
    QnResourcePtr res,
    nxcip::BaseCameraManager* camManager )
:
    CLServerPushStreamreader( res ),
    m_rtpStreamParser( res ),
    m_camManager( camManager )
{
    m_thirdPartyRes = getResource().dynamicCast<QnThirdPartyResource>();
    Q_ASSERT( m_thirdPartyRes );
}

ThirdPartyStreamReader::~ThirdPartyStreamReader()
{
    stop();
}

static const nxcip::Resolution DEFAULT_SECOND_STREAM_RESOLUTION = nxcip::Resolution(480, 316);

void ThirdPartyStreamReader::openStream()
{
    if( isStreamOpened() )
        return;

    QnResource::ConnectionRole role = getRole();
    const int encoderNumber = role == QnResource::Role_LiveVideo ? 0 : 1;

    nxcip::CameraMediaEncoder* intf = NULL;
    int result = m_camManager.getEncoder( encoderNumber, &intf );
    if( result != nxcip::NX_NO_ERROR )
        return;
    nxcip_qt::CameraMediaEncoder cameraEncoder( intf );

    if( m_camManager.setAudioEnabled( m_thirdPartyRes->isAudioEnabled() ) != nxcip::NX_NO_ERROR )
        return;

    const nxcip::Resolution& resolution = (role == QnResource::Role_LiveVideo) 
        ? getMaxResolution(encoderNumber) 
        : getNearestResolution(encoderNumber, DEFAULT_SECOND_STREAM_RESOLUTION);
    if( resolution.width*resolution.height > 0 )
        if( cameraEncoder.setResolution( resolution ) != nxcip::NX_NO_ERROR )
            return;
    float selectedFps = 0;
    if( cameraEncoder.setFps( getFps(), &selectedFps ) != nxcip::NX_NO_ERROR )
        return;

    QString mediaUrlStr;
    if( cameraEncoder.getMediaUrl( &mediaUrlStr ) != nxcip::NX_NO_ERROR )
        return;

    m_rtpStreamParser.setRequest( mediaUrlStr );
    m_rtpStreamParser.openStream();
}

void ThirdPartyStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool ThirdPartyStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr ThirdPartyStreamReader::getCameraMetadata()
{
    //TODO/IMPL

    QnMetaDataV1Ptr rez = m_lastMetadata != 0 ? m_lastMetadata : QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastMetadata.clear();
    return rez;
}

void ThirdPartyStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_rtpStreamParser.pleaseStop();
}

QnAbstractMediaDataPtr ThirdPartyStreamReader::getNextData()
{
    if( getRole() == QnResource::Role_LiveVideo )
        readMotionInfo();

    if( !isStreamOpened() )
    {
        openStream();
        if( !isStreamOpened() )
            return QnAbstractMediaDataPtr(0);
    }

    if( needMetaData() )
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    static const int MAX_TRIES_TO_READ_MEDIA_PACKET = 10;
    for( int i = 0; i < MAX_TRIES_TO_READ_MEDIA_PACKET; ++i )
    {
        rez = m_rtpStreamParser.getNextData();
        if( rez )
        {
            if( rez->dataType == QnAbstractMediaData::VIDEO )
            {
                QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
                //parseMotionInfo(videoData);
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

void ThirdPartyStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void ThirdPartyStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

const QnResourceAudioLayout* ThirdPartyStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

static bool resolutionLess( const nxcip::Resolution& left, const nxcip::Resolution& right )
{
    return left.width*left.height < right.width*right.height;
}

nxcip::Resolution ThirdPartyStreamReader::getMaxResolution( int encoderNumber ) const
{
    const QList<nxcip::Resolution>& resolutionList = m_thirdPartyRes->getEncoderResolutionList( encoderNumber );
    return *std::max_element( resolutionList.begin(), resolutionList.end(), resolutionLess );
}

nxcip::Resolution ThirdPartyStreamReader::getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const
{
    const QList<nxcip::Resolution>& resolutionList = m_thirdPartyRes->getEncoderResolutionList( encoderNumber );
    nxcip::Resolution foundResolution;
    foreach( nxcip::Resolution resolution, resolutionList )
    {
        if( resolution.width*resolution.height <= desiredResolution.width*desiredResolution.height &&
            resolution.width*resolution.height > foundResolution.width*foundResolution.height )
        {
            foundResolution = resolution;
        }
    }
    return foundResolution;
}

void ThirdPartyStreamReader::readMotionInfo()
{
    //TODO/IMPL reading motion info using nxcip::CameraMotionDataProvider
}
