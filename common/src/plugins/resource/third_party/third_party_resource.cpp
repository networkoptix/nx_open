/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_resource.h"

#ifdef ENABLE_THIRD_PARTY

#include <functional>
#include <memory>

#include <QtCore/QStringList>

#include <core/resource/camera_advanced_param.h>

#include <utils/common/log.h>

#include "api/app_server_connection.h"
#include "motion_data_picture.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "third_party_archive_delegate.h"
#include "third_party_ptz_controller.h"
#include "third_party_stream_reader.h"


static const QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
static const float DEFAULT_MAX_FPS_IN_CASE_IF_UNKNOWN = 30.0;
const QString QnThirdPartyResource::AUX_DATA_PARAM_NAME = QLatin1String("aux_data");

QnThirdPartyResource::QnThirdPartyResource(
    const nxcip::CameraInfo& camInfo,
    nxcip::BaseCameraManager* camManager,
    const nxcip_qt::CameraDiscoveryManager& discoveryManager )
:
    m_camInfo( camInfo ),
    m_camManager( camManager ? new nxcip_qt::BaseCameraManager(camManager) : nullptr ),
    m_discoveryManager( discoveryManager ),
    m_refCounter( 2 ),
    m_encoderCount( 0 ),
    m_cameraManager3( nullptr )
{
    setVendor( discoveryManager.getVendorName() );
    setDefaultAuth(QString::fromUtf8(camInfo.defaultLogin), QString::fromUtf8(camInfo.defaultPassword));

    if( m_camManager )
        m_cameraManager3 = (nxcip::BaseCameraManager3*)m_camManager->getRef()->queryInterface( nxcip::IID_BaseCameraManager3 );
}

QnThirdPartyResource::~QnThirdPartyResource()
{
    if( m_cameraManager3 )
    {
        m_cameraManager3->releaseRef();
        m_cameraManager3 = nullptr;
    }

    stopInputPortMonitoring();
}

QnAbstractPtzController* QnThirdPartyResource::createPtzControllerInternal()
{
    if( !m_camManager )
        return nullptr;
    nxcip::CameraPtzManager* ptzManager = m_camManager->getPtzManager();
    if( !ptzManager )
        return NULL;
    return new QnThirdPartyPtzController( toSharedPointer().staticCast<QnThirdPartyResource>(), ptzManager );
}

bool QnThirdPartyResource::getParamPhysical(const QString& id, QString &value) {
	qDebug() << "QnThirdPartyResource::getParamPhysical" << id;
    QMutexLocker lk( &m_mutex );

    if( !m_cameraManager3 )
        return false;

    static const size_t DEFAULT_PARAM_VALUE_BUF_SIZE = 256;
    int valueBufSize = DEFAULT_PARAM_VALUE_BUF_SIZE;
    std::unique_ptr<char[]> valueBuf( new char[valueBufSize] );

    int result = nxcip::NX_NO_ERROR;
    for( int i = 0; i < 2; ++i )
    {
        result = m_cameraManager3->getParamValue(
            id.toUtf8().constData(),
            valueBuf.get(),
            &valueBufSize );
        switch( result )
        {
            case nxcip::NX_NO_ERROR:
            {
                value = QString::fromUtf8( valueBuf.get(), valueBufSize );
				qDebug() << "QnThirdPartyResource::getParamPhysical calculated result" << id << value << value.size();
                return true;
            }
            case nxcip::NX_MORE_DATA:
                valueBuf.reset( new char[valueBufSize] );
                continue;
            default:
                break;
        }
        break;
    }

    //TODO #ak return error description
	qDebug() << "QnThirdPartyResource::getParamPhysical could not calculate result for" << id;
    return false;
}

bool QnThirdPartyResource::setParamPhysical(const QString& id, const QString &value) {
    QMutexLocker lk( &m_mutex );
    if( !m_cameraManager3 )
        return false;

    return m_cameraManager3->setParamValue(
        id.toUtf8().constData(),
        value.toUtf8().constData() ) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyResource::ping()
{
    //TODO: should check if camera supports http and, if supports, check http port
    return true;
}

QString QnThirdPartyResource::getDriverName() const
{
    return m_discoveryManager.getVendorName();
}

void QnThirdPartyResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnThirdPartyResource::createLiveDataProvider()
{
    if( !m_camManager )
        return nullptr;
    m_camManager->getRef()->addRef();
    return new ThirdPartyStreamReader( toSharedPointer(), m_camManager->getRef() );
}

void QnThirdPartyResource::setMotionMaskPhysical(int /*channel*/)
{
    //TODO/IMPL
}

QnConstResourceAudioLayoutPtr QnThirdPartyResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled()) {
        const ThirdPartyStreamReader* reader = dynamic_cast<const ThirdPartyStreamReader*>(dataProvider);
        if (reader && reader->getDPAudioLayout())
            return reader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

//!Implementation of QnSecurityCamResource::getRelayOutputList
QStringList QnThirdPartyResource::getRelayOutputList() const
{
    QStringList ids;
    if( !m_relayIOManager.get() )
        return ids;

    m_relayIOManager->getRelayOutputList( &ids );
    return ids;
}

QStringList QnThirdPartyResource::getInputPortList() const
{
    QStringList ids;
    if( !m_relayIOManager.get() )
        return ids;

    m_relayIOManager->getInputPortList( &ids );
    return ids;
}

bool QnThirdPartyResource::setRelayOutputState(
    const QString& outputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    if( !m_relayIOManager.get() )
        return false;

    //if outputID is empty, activating first port in the port list
    return m_relayIOManager->setRelayOutputState(
        outputID.isEmpty() ? m_defaultOutputID : outputID,
        activate,
        autoResetTimeoutMS ) == nxcip::NX_NO_ERROR;
}

QnAbstractStreamDataProvider* QnThirdPartyResource::createArchiveDataProvider()
{
    QnAbstractArchiveDelegate* archiveDelegate = createArchiveDelegate();
    if( !archiveDelegate )
        return QnPhysicalCameraResource::createArchiveDataProvider();
    QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(toSharedPointer());
    archiveReader->setArchiveDelegate(archiveDelegate);
    return archiveReader;
}

QnAbstractArchiveDelegate* QnThirdPartyResource::createArchiveDelegate()
{
    if( !m_camManager )
        return nullptr;

    unsigned int camCapabilities = 0;
    if( m_camManager->getCameraCapabilities( &camCapabilities ) != nxcip::NX_NO_ERROR ||
        (camCapabilities & nxcip::BaseCameraManager::dtsArchiveCapability) == 0 )
    {
        return NULL;
    }

    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager->getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
    if( !camManager2 )
        return NULL;

    nxcip::DtsArchiveReader* archiveReader = NULL;
    if( camManager2->createDtsArchiveReader( &archiveReader ) != nxcip::NX_NO_ERROR ||
        archiveReader == NULL )
    {
        return NULL;
    }
    camManager2->releaseRef();

    return new ThirdPartyArchiveDelegate( toResourcePtr(), archiveReader );
}

static const unsigned int USEC_IN_MS = 1000;

QnTimePeriodList QnThirdPartyResource::getDtsTimePeriodsByMotionRegion(
    const QList<QRegion>& regions,
    qint64 startTimeMs,
    qint64 endTimeMs,
    int detailLevel )
{
    if( !m_camManager )
        return QnTimePeriodList();

    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager->getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
    Q_ASSERT( camManager2 );

    QnTimePeriodList resultTimePeriods;

    nxcip::ArchiveSearchOptions searchOptions;
    if( !regions.isEmpty() )
    {
        //filling in motion mask
        std::auto_ptr<MotionDataPicture> motionDataPicture( new MotionDataPicture( nxcip::PIX_FMT_MONOBLACK ) );

        QRegion unitedRegion;
        for( QList<QRegion>::const_iterator
            it = regions.begin();
            it != regions.end();
            ++it )
        {
            unitedRegion = unitedRegion.united( *it );
        }

        const QVector<QRect>& rects = unitedRegion.rects();
        for(const  QRect& r: rects )
        {
            for( int y = r.top(); y < std::min<int>(motionDataPicture->height(), r.bottom()); ++y )
                for( int x = r.left(); x < std::min<int>(motionDataPicture->width(), r.right()); ++x )
                    motionDataPicture->setPixel( x, y, 1 ); //TODO: some optimization would be appropriate
        }

        searchOptions.motionMask = motionDataPicture.release();
    }
    searchOptions.startTime = startTimeMs * USEC_IN_MS;
    searchOptions.endTime = endTimeMs * USEC_IN_MS;
    searchOptions.periodDetailLevel = detailLevel;
    nxcip::TimePeriods* timePeriods = NULL;
    if( camManager2->find( &searchOptions, &timePeriods ) != nxcip::NX_NO_ERROR || !timePeriods )
        return resultTimePeriods;
    camManager2->releaseRef();

    for( timePeriods->goToBeginning(); !timePeriods->atEnd(); timePeriods->next() )
    {
        nxcip::UsecUTCTimestamp periodStart = nxcip::INVALID_TIMESTAMP_VALUE;
        nxcip::UsecUTCTimestamp periodEnd = nxcip::INVALID_TIMESTAMP_VALUE;
        timePeriods->get( &periodStart, &periodEnd );

        resultTimePeriods << QnTimePeriod( periodStart / USEC_IN_MS, (periodEnd-periodStart) / USEC_IN_MS );
    }
    timePeriods->releaseRef();

    return resultTimePeriods;
}

QnTimePeriodList QnThirdPartyResource::getDtsTimePeriods( qint64 startTimeMs, qint64 endTimeMs, int detailLevel )
{
    return getDtsTimePeriodsByMotionRegion(
        QList<QRegion>(),
        startTimeMs,
        endTimeMs,
        detailLevel );
}

//!Implementation of nxpl::NXPluginInterface::queryInterface
void* QnThirdPartyResource::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraInputEventHandler, sizeof(nxcip::IID_CameraInputEventHandler) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraInputEventHandler *>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::NXPluginInterface::queryInterface
unsigned int QnThirdPartyResource::addRef()
{
    return m_refCounter.fetchAndAddOrdered(1) + 1;
}

//!Implementation of nxpl::NXPluginInterface::queryInterface
unsigned int QnThirdPartyResource::releaseRef()
{
    return m_refCounter.fetchAndAddOrdered(-1) - 1;
}

void QnThirdPartyResource::inputPortStateChanged(
    nxcip::CameraRelayIOManager* /*source*/,
    const char* inputPortID,
    int newState,
    unsigned long int /*timestamp*/ )
{
    emit cameraInput(
        toSharedPointer(),
        QString::fromUtf8(inputPortID),
        newState != 0,
        QDateTime::currentMSecsSinceEpoch() );
}

const QList<nxcip::Resolution>& QnThirdPartyResource::getEncoderResolutionList( int encoderNumber ) const
{
    QMutexLocker lk( &m_mutex );
    return m_encoderData[encoderNumber].resolutionList;
}


bool QnThirdPartyResource::hasDualStreaming() const
{
    return m_encoderCount > 1;
}

nxcip::Resolution QnThirdPartyResource::getSelectedResolutionForEncoder( int encoderIndex ) const
{
    QMutexLocker lk( &m_mutex );
    if( (size_t)encoderIndex < m_selectedEncoderResolutions.size() )
        return m_selectedEncoderResolutions[encoderIndex];
    return nxcip::Resolution();
}

CameraDiagnostics::Result QnThirdPartyResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();

    if( !m_camManager )
    {
        QMutexLocker lk( &m_mutex );

        //restoring camera parameters
        if( strlen(m_camInfo.uid) == 0 )
        {
            memset( m_camInfo.uid, 0, sizeof(m_camInfo.uid) );
            const QByteArray& physicalId = getPhysicalId().toLatin1();
            strncpy( m_camInfo.uid, physicalId.constData(), std::min<size_t>(physicalId.size(), sizeof(m_camInfo.uid)-1) );
        }
        if( strlen(m_camInfo.modelName) == 0 )
        {
            memset( m_camInfo.modelName, 0, sizeof(m_camInfo.modelName) );
            const QByteArray& model = getModel().toLatin1();
            strncpy( m_camInfo.modelName, model.constData(), std::min<size_t>(model.size(), sizeof(m_camInfo.modelName)-1) );
        }
        if( strlen(m_camInfo.firmware) == 0 )
        {
            memset( m_camInfo.firmware, 0, sizeof(m_camInfo.firmware) );
            const QByteArray& firmware = getFirmware().toLatin1();
            strncpy( m_camInfo.firmware, firmware.constData(), std::min<size_t>(firmware.size(), sizeof(m_camInfo.firmware)-1) );
        }
        if( strlen(m_camInfo.auxiliaryData) == 0 )
        {
            const QString& auxDataStr = getProperty( AUX_DATA_PARAM_NAME );
            if( !auxDataStr.isEmpty() )
            {
                memset( m_camInfo.auxiliaryData, 0, sizeof(m_camInfo.auxiliaryData) );
                const QByteArray& auxData = auxDataStr.toLatin1();
                strncpy( m_camInfo.auxiliaryData, auxData.constData(), std::min<size_t>(auxData.size(), sizeof(m_camInfo.auxiliaryData)-1) );
            }
        }
        if( strlen(m_camInfo.defaultLogin) == 0 )
        {
            const QByteArray userName = getAuth().user().toLatin1();
            strncpy( m_camInfo.defaultLogin, userName.constData(), std::min<size_t>(userName.size(), sizeof(m_camInfo.defaultLogin)-1) );
        }
        if( strlen(m_camInfo.defaultPassword) == 0 )
        {
            const QByteArray userPassword = getAuth().password().toLatin1();
            strncpy( m_camInfo.defaultPassword, userPassword.constData(), std::min<size_t>(userPassword.size(), sizeof(m_camInfo.defaultPassword)-1) );
        }

        nxcip::BaseCameraManager* cameraIntf = m_discoveryManager.createCameraManager( m_camInfo );
        if( !cameraIntf )
            return CameraDiagnostics::UnknownErrorResult();
        m_camManager.reset( new nxcip_qt::BaseCameraManager( cameraIntf ) );

        m_cameraManager3 = (nxcip::BaseCameraManager3*)m_camManager->getRef()->queryInterface( nxcip::IID_BaseCameraManager3 );
    }

    m_camManager->setCredentials( getAuth().user(), getAuth().password() );

    int result = m_camManager->getCameraInfo( &m_camInfo );
    if( result != nxcip::NX_NO_ERROR )
    {
        if( false )
        NX_LOG( lit("Error getting camera info from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager->getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }

    setFirmware( QString::fromUtf8(m_camInfo.firmware) );

    m_encoderCount = 0;
    result = m_camManager->getEncoderCount( &m_encoderCount );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( lit("Error getting encoder count from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager->getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }

    if( m_encoderCount == 0 )
    {
        NX_LOG( lit("Third-party camera %1:%2 (url %3) returned 0 encoder count!").arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logDEBUG1 );
        return CameraDiagnostics::UnknownErrorResult();
    }

    //we support only two streams from camera
    m_encoderCount = m_encoderCount > 2 ? 2 : m_encoderCount;

    setProperty( Qn::HAS_DUAL_STREAMING_PARAM_NAME, (m_encoderCount > 1) ? 1 : 0);

    //setting camera capabilities
    unsigned int cameraCapabilities = 0;
    result = m_camManager->getCameraCapabilities( &cameraCapabilities );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( lit("Error reading camera capabilities from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager->getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability )
        setCameraCapability( Qn::RelayInputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability )
        setCameraCapability( Qn::RelayOutputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::shareIpCapability )
        setCameraCapability( Qn::ShareIpCapability, true );
    //if( cameraCapabilities & nxcip::BaseCameraManager::primaryStreamSoftMotionCapability )
    //    setCameraCapability( Qn::PrimaryStreamSoftMotionCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::ptzCapability )
    {
        nxcip::CameraPtzManager* ptzManager = m_camManager->getPtzManager();
        if( ptzManager )
        {
            const int ptzCapabilities = ptzManager->getCapabilities();
            if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousPanCapability )
                setPtzCapability( Qn::ContinuousPanCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousTiltCapability )
                setPtzCapability( Qn::ContinuousTiltCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousZoomCapability )
                setPtzCapability( Qn::ContinuousZoomCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::AbsolutePanCapability )
                setPtzCapability( Qn::AbsolutePanCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteTiltCapability )
                setPtzCapability( Qn::AbsoluteTiltCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteZoomCapability )
                setPtzCapability( Qn::AbsoluteZoomCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::FlipPtzCapability )
                setPtzCapability( Qn::FlipPtzCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::LimitsPtzCapability )
                setPtzCapability( Qn::LimitsPtzCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::DevicePositioningPtzCapability )
                setPtzCapability( Qn::DevicePositioningPtzCapability, true );
            if( ptzCapabilities & nxcip::CameraPtzManager::LogicalPositioningPtzCapability )
                setPtzCapability( Qn::LogicalPositioningPtzCapability, true );
        }
        ptzManager->releaseRef();
    }
    setProperty(
        Qn::IS_AUDIO_SUPPORTED_PARAM_NAME,
        (cameraCapabilities & nxcip::BaseCameraManager::audioCapability) ? 1 : 0);
    if( cameraCapabilities & nxcip::BaseCameraManager::dtsArchiveCapability )
    {
        setProperty( Qn::DTS_PARAM_NAME, 1);
        setProperty( Qn::ANALOG_PARAM_NAME, 1);
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::hardwareMotionCapability )
    {
        //setMotionType( Qn::MT_HardwareGrid );
        setProperty( Qn::MOTION_WINDOW_CNT_PARAM_NAME, 100);
        setProperty( Qn::MOTION_MASK_WINDOW_CNT_PARAM_NAME, 100);
        setProperty( Qn::MOTION_SENS_WINDOW_CNT_PARAM_NAME, 100);
        setProperty( Qn::SUPPORTED_MOTION_PARAM_NAME, QStringLiteral("softwaregrid,hardwaregrid"));
    }
    else
    {
        //setMotionType( Qn::MT_SoftwareGrid );
        setProperty( Qn::SUPPORTED_MOTION_PARAM_NAME, QStringLiteral("softwaregrid"));
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::shareFpsCapability )
		setStreamFpsSharingMethod(Qn::BasicFpsSharing);
    else if( cameraCapabilities & nxcip::BaseCameraManager::sharePixelsCapability )
		setStreamFpsSharingMethod(Qn::PixelsFpsSharing);
    else 
        setStreamFpsSharingMethod(Qn::NoFpsSharing);
        

    QVector<EncoderData> encoderDataTemp;
    encoderDataTemp.resize( m_encoderCount );

    //reading resolution list
    QVector<nxcip::ResolutionInfo> resolutionInfoList;
    float maxFps = 0;
    for( int encoderNumber = 0; encoderNumber < m_encoderCount; ++encoderNumber )
    {
        //const int result = m_camManager->getResolutionList( i, &resolutionInfoList );
        nxcip::CameraMediaEncoder* intf = NULL;
        int result = m_camManager->getEncoder( encoderNumber, &intf );
        if( result == nxcip::NX_NO_ERROR )
        {
            nxcip_qt::CameraMediaEncoder cameraEncoder( intf );
            resolutionInfoList.clear();
            result = cameraEncoder.getResolutionList( &resolutionInfoList );
        }
        if( result != nxcip::NX_NO_ERROR )
        {
            NX_LOG( lit("Failed to get resolution list of third-party camera %1:%2 encoder %3. %4").
                arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
                arg(encoderNumber).arg(m_camManager->getLastErrorString()), cl_logDEBUG1 );
            if( result == nxcip::NX_NOT_AUTHORIZED )
                setStatus( Qn::Unauthorized );
            return CameraDiagnostics::CannotConfigureMediaStreamResult(lit("resolution"));
        }
        for( int j = 0; j < resolutionInfoList.size(); ++j )
        {
            encoderDataTemp[encoderNumber].resolutionList.push_back( resolutionInfoList[j].resolution );
            if( resolutionInfoList[j].maxFps > maxFps )
                maxFps = resolutionInfoList[j].maxFps;
        }
    }

    {
        QMutexLocker lk( &m_mutex );
        m_encoderData = encoderDataTemp;
    }

    if( !maxFps )
        maxFps = DEFAULT_MAX_FPS_IN_CASE_IF_UNKNOWN;

    setProperty( MAX_FPS_PARAM_NAME, maxFps);

    if( (cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability) ||
        (cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability) )
    {
        initializeIOPorts();
    }

    //TODO #ak: current API does not allow to get stream codec, so using CODEC_ID_H264 as true in most cases
    CameraMediaStreams mediaStreams;
    std::vector<nxcip::Resolution> selectedEncoderResolutions( m_encoderCount );
    selectedEncoderResolutions[PRIMARY_ENCODER_INDEX] = getMaxResolution( PRIMARY_ENCODER_INDEX );
    mediaStreams.streams.push_back( CameraMediaStreamInfo(
        QSize(selectedEncoderResolutions[PRIMARY_ENCODER_INDEX].width, selectedEncoderResolutions[PRIMARY_ENCODER_INDEX].height),
        CODEC_ID_H264 ) );
    if( SECONDARY_ENCODER_INDEX < m_encoderCount )
    {
        selectedEncoderResolutions[SECONDARY_ENCODER_INDEX] = getSecondStreamResolution();
        mediaStreams.streams.push_back( CameraMediaStreamInfo(
            QSize(selectedEncoderResolutions[SECONDARY_ENCODER_INDEX].width, selectedEncoderResolutions[SECONDARY_ENCODER_INDEX].height),
            CODEC_ID_H264 ) );
    }

    if( m_cameraManager3 )
    {
        //reading camera parameters description (if any)
        const char* paramDescXMLStr = m_cameraManager3->getParametersDescriptionXML();
        if( paramDescXMLStr != nullptr )
        {
            QByteArray paramDescXML = QByteArray::fromRawData( paramDescXMLStr, strlen(paramDescXMLStr) );
            QBuffer dataSource(&paramDescXML);

            if( QnCameraAdvacedParamsXmlParser::validateXml(&dataSource)) {
                //parsing xml to load param list and get cameraID
                QnCameraAdvancedParams params;
                QString cameraTypeName;
				if (QnCameraAdvacedParamsXmlParser::readXml(&dataSource, params, cameraTypeName)) {
                    setProperty(Qn::CAMERA_ADVANCED_PARAMS_XML, QString::fromUtf8(paramDescXML));
                    setProperty(Qn::CAMERA_ADVANCED_PARAMS_TYPENAME, cameraTypeName);
                    setProperty(Qn::CAMERA_ADVANCED_PARAMS_SUPPORTED, QStringList(params.allParameterIds().toList()).join(L','));
                }
            }
            else
            {
                NX_LOG( lit("Could not validate camera parameters description xml"), cl_logWARNING );
            }
        }
    }

    {
        QMutexLocker lk( &m_mutex );
        m_selectedEncoderResolutions = std::move( selectedEncoderResolutions );
    }

    saveResolutionList( mediaStreams );

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnThirdPartyResource::startInputPortMonitoring()
{
    if( !m_relayIOManager.get() )
        return false;
    m_relayIOManager->registerEventHandler( this );
    return m_relayIOManager->startInputPortMonitoring() == nxcip::NX_NO_ERROR;
}

void QnThirdPartyResource::stopInputPortMonitoring()
{
    if( m_relayIOManager.get() )
    {
        m_relayIOManager->stopInputPortMonitoring();
        m_relayIOManager->unregisterEventHandler( this );
    }
}

bool QnThirdPartyResource::isInputPortMonitored() const
{
    //TODO/IMPL
    return false;
}

bool QnThirdPartyResource::initializeIOPorts()
{
    //initializing I/O
    nxcip::CameraRelayIOManager* camIOManager = m_camManager->getCameraRelayIOManager();
    if( !camIOManager )
    {
        NX_LOG( lit("Failed to get pointer to nxcip::CameraRelayIOManager interface for third-party camera %1:%2 (url %3)").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logWARNING );
        setCameraCapability( Qn::RelayInputCapability, false );
        setCameraCapability( Qn::RelayOutputCapability, false );
        return false;
    }

    m_relayIOManager.reset( new nxcip_qt::CameraRelayIOManager( camIOManager ) );

    QStringList outputPortList;
    int result = m_relayIOManager->getRelayOutputList( &outputPortList );
    if( result )
        return result;
    if( !outputPortList.isEmpty() )
        m_defaultOutputID = outputPortList[0];

    return true;
}

nxcip::Resolution QnThirdPartyResource::getMaxResolution( int encoderNumber ) const
{
    const QList<nxcip::Resolution>& resolutionList = getEncoderResolutionList( encoderNumber );
    QList<nxcip::Resolution>::const_iterator maxResIter = std::max_element(
        resolutionList.constBegin(),
        resolutionList.constEnd(),
        [](const nxcip::Resolution& left, const nxcip::Resolution& right){
            return left.width*left.height < right.width*right.height;
    } );
    return maxResIter != resolutionList.constEnd() ? *maxResIter : nxcip::Resolution();
}

nxcip::Resolution QnThirdPartyResource::getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const
{
    const QList<nxcip::Resolution>& resolutionList = getEncoderResolutionList( encoderNumber );
    nxcip::Resolution foundResolution;
    for(const  nxcip::Resolution& resolution: resolutionList )
    {
        if( resolution.width*resolution.height <= desiredResolution.width*desiredResolution.height &&
            resolution.width*resolution.height > foundResolution.width*foundResolution.height )
        {
            foundResolution = resolution;
        }
    }
    return foundResolution;
}

static const nxcip::Resolution DEFAULT_SECOND_STREAM_RESOLUTION = nxcip::Resolution(480, 316);

nxcip::Resolution QnThirdPartyResource::getSecondStreamResolution() const
{
    const nxcip::Resolution& primaryStreamResolution = getMaxResolution( PRIMARY_ENCODER_INDEX );
    const float currentAspect = QnPhysicalCameraResource::getResolutionAspectRatio( QSize(primaryStreamResolution.width, primaryStreamResolution.height) );

    const QList<nxcip::Resolution>& resolutionList = getEncoderResolutionList( SECONDARY_ENCODER_INDEX );
    if( resolutionList.isEmpty() )
        return DEFAULT_SECOND_STREAM_RESOLUTION;

    //preparing data in format suitable for getNearestResolution
    QList<QSize> resList;
    std::transform(
        resolutionList.begin(), resolutionList.end(), std::back_inserter(resList),
        []( const nxcip::Resolution& resolution ){ return QSize(resolution.width, resolution.height); } );

    QSize secondaryResolution = QnPhysicalCameraResource::getNearestResolution(
        QSize(DEFAULT_SECOND_STREAM_RESOLUTION.width, DEFAULT_SECOND_STREAM_RESOLUTION.height),
        currentAspect,
        SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height(),
        resList );
    if( secondaryResolution == EMPTY_RESOLUTION_PAIR )
        secondaryResolution = QnPhysicalCameraResource::getNearestResolution(
            QSize(DEFAULT_SECOND_STREAM_RESOLUTION.width, DEFAULT_SECOND_STREAM_RESOLUTION.height),
            0.0,        //ignoring aspect ratio
            SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height(),
            resList );

    return nxcip::Resolution( secondaryResolution.width(), secondaryResolution.height() );
}

#endif // ENABLE_THIRD_PARTY
