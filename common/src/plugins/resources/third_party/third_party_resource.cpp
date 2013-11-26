/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_resource.h"

#include <functional>
#include <memory>

#include <QtCore/QStringList>

#include "api/app_server_connection.h"
#include "motion_data_picture.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "third_party_stream_reader.h"
#include "third_party_archive_delegate.h"


static const QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");

QnThirdPartyResource::QnThirdPartyResource(
    const nxcip::CameraInfo& camInfo,
    nxcip::BaseCameraManager* camManager,
    const nxcip_qt::CameraDiscoveryManager& discoveryManager )
:
    m_camInfo( camInfo ),
    m_camManager( camManager ),
    m_discoveryManager( discoveryManager ),
    m_refCounter( 2 )
{
    setAuth( QString::fromUtf8(camInfo.defaultLogin), QString::fromUtf8(camInfo.defaultPassword) );
}

QnThirdPartyResource::~QnThirdPartyResource()
{
    stopInputPortMonitoring();
}

QnAbstractPtzController *QnThirdPartyResource::createPtzControllerInternal()
{
    //TODO/IMPL
    return NULL;
}

bool QnThirdPartyResource::isResourceAccessible()
{
    return updateMACAddress();
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
    m_camManager.getRef()->addRef();
    return new ThirdPartyStreamReader( toSharedPointer(), m_camManager.getRef() );
}

void QnThirdPartyResource::setCropingPhysical(QRect /*croping*/)
{

}

void QnThirdPartyResource::setMotionMaskPhysical(int /*channel*/)
{
    //TODO/IMPL
}

const QnResourceAudioLayout* QnThirdPartyResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
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
        return QnSecurityCamResource::createArchiveDataProvider();
        return QnPhysicalCameraResource::createArchiveDataProvider();
    QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(toSharedPointer());
    archiveReader->setArchiveDelegate(archiveDelegate);
    if (hasFlags(still_image) || hasFlags(utc))
        archiveReader->setCycleMode(false);
    return archiveReader;
}

QnAbstractArchiveDelegate* QnThirdPartyResource::createArchiveDelegate()
{
    unsigned int camCapabilities = 0;
    if( m_camManager.getCameraCapabilities( &camCapabilities ) != nxcip::NX_NO_ERROR ||
        (camCapabilities & nxcip::BaseCameraManager::dtsArchiveCapability) == 0 )
    {
        return NULL;
    }

    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager.getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
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
    nxcip::BaseCameraManager2* camManager2 = static_cast<nxcip::BaseCameraManager2*>(m_camManager.getRef()->queryInterface( nxcip::IID_BaseCameraManager2 ));
    Q_ASSERT( camManager2 );

    QnTimePeriodList resultTimePeriods;

    nxcip::ArchiveSearchOptions searchOptions;
    if( !regions.isEmpty() )
    {
        //filling in motion mask
        std::auto_ptr<MotionDataPicture> motionDataPicture( new MotionDataPicture() );

        QRegion unitedRegion;
        for( QList<QRegion>::const_iterator
            it = regions.begin();
            it != regions.end();
            ++it )
        {
            unitedRegion = unitedRegion.united( *it );
        }

        const QVector<QRect>& rects = unitedRegion.rects();
        foreach( QRect r, rects )
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

CameraDiagnostics::Result QnThirdPartyResource::initInternal()
{
    m_camManager.setCredentials( getAuth().user(), getAuth().password() );

    int result = m_camManager.getCameraInfo( &m_camInfo );
    if( result != nxcip::NX_NO_ERROR )
    {
        if( false )
        NX_LOG( QString::fromLatin1("Error getting camera info from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }

    setFirmware( QString::fromUtf8(m_camInfo.firmware) );

    int encoderCount = 0;
    result = m_camManager.getEncoderCount( &encoderCount );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( QString::fromLatin1("Error getting encoder count from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }

    if( encoderCount == 0 )
    {
        NX_LOG( QString::fromLatin1("Third-party camera %1:%2 (url %3) returned 0 encoder count!").arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logDEBUG1 );
        return CameraDiagnostics::UnknownErrorResult();
    }

    setParam( lit("hasDualStreaming"), encoderCount > 1, QnDomainDatabase );

    //setting camera capabilities
    unsigned int cameraCapabilities = 0;
    result = m_camManager.getCameraCapabilities( &cameraCapabilities );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( QString::fromLatin1("Error reading camera capabilities from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getLastErrorString()), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability )
        setCameraCapability( Qn::RelayInputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability )
        setCameraCapability( Qn::RelayOutputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::shareIpCapability )
        setCameraCapability( Qn::shareIpCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::primaryStreamSoftMotionCapability )
        setCameraCapability( Qn::PrimaryStreamSoftMotionCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::ptzCapability )
    {
        //setPtzCapability( Qn::AbsolutePtzCapability, true );
        //TODO/IMPL requesting nxcip::CameraPTZManager interface and setting capabilities
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::audioCapability )
        setAudioEnabled( true );
    if( cameraCapabilities & nxcip::BaseCameraManager::dtsArchiveCapability )
        setParam( lit("dts"), 1, QnDomainMemory );
    //if( cameraCapabilities & nxcip::BaseCameraManager::shareFpsCapability )
    //    setCameraCapability( Qn:: );
    //if( cameraCapabilities & nxcip::BaseCameraManager::sharePixelsCapability )
    //    setCameraCapability( Qn:: );

    QVector<EncoderData> encoderDataTemp;
    encoderDataTemp.resize( encoderCount );

    //reading resolution list
    QVector<nxcip::ResolutionInfo> resolutionInfoList;
    float maxFps = 0;
    for( int encoderNumber = 0; encoderNumber < encoderCount; ++encoderNumber )
    {
        //const int result = m_camManager.getResolutionList( i, &resolutionInfoList );
        nxcip::CameraMediaEncoder* intf = NULL;
        int result = m_camManager.getEncoder( encoderNumber, &intf );
        if( result == nxcip::NX_NO_ERROR )
        {
            nxcip_qt::CameraMediaEncoder cameraEncoder( intf );
            resolutionInfoList.clear();
            result = cameraEncoder.getResolutionList( &resolutionInfoList );
        }
        if( result != nxcip::NX_NO_ERROR )
        {
            NX_LOG( QString::fromLatin1("Failed to get resolution list of third-party camera %1:%2 encoder %3. %4").
                arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
                arg(encoderNumber).arg(m_camManager.getLastErrorString()), cl_logDEBUG1 );
            if( result == nxcip::NX_NOT_AUTHORIZED )
                setStatus( QnResource::Unauthorized );
            return CameraDiagnostics::UnknownErrorResult();
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

    if( !setParam( MAX_FPS_PARAM_NAME, maxFps, QnDomainDatabase ) )
    {
        NX_LOG( QString::fromLatin1("Failed to set %1 parameter to %2 for third-party camera %3:%4 (url %5)").
            arg(MAX_FPS_PARAM_NAME).arg(maxFps).arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logDEBUG1 );
    }

    if( (cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability) ||
        (cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability) )
    {
        initializeIOPorts();
    }

    //TODO/IMPL initializing PTZ

    // TODO: #Elric this is totally evil, copypasta from ONVIF resource.
    {
        QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
        if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>()) != 0)
            qnCritical("Can't save resource %1 to Enterprise Controller. Error: %2.", getName(), conn->getLastError());
    }

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
    nxcip::CameraRelayIOManager* camIOManager = m_camManager.getCameraRelayIOManager();
    if( !camIOManager )
    {
        NX_LOG( QString::fromLatin1("Failed to get pointer to nxcip::CameraRelayIOManager interface for third-party camera %1:%2 (url %3)").
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
