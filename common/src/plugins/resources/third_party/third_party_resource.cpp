/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "third_party_resource.h"

#include <functional>
#include <memory>

#include <business/business_event_connector.h>

#include "api/app_server_connection.h"
#include "third_party_stream_reader.h"


static const QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");

QnThirdPartyResource::QnThirdPartyResource(
    const nxcip::CameraInfo& camInfo,
    nxcip::BaseCameraManager* camManager,
    const nxcip_qt::CameraDiscoveryManager& discoveryManager )
:
    m_camInfo( camInfo ),
    m_camManager( camManager ),
    m_discoveryManager( discoveryManager )
{
    setAuth( QString::fromUtf8(camInfo.defaultLogin), QString::fromUtf8(camInfo.defaultPassword) );
    connect(
        this, SIGNAL(cameraInput(QnResourcePtr, const QString&, bool, qint64)), 
        QnBusinessEventConnector::instance(), SLOT(at_cameraInput(QnResourcePtr, const QString&, bool, qint64)) );
}

QnThirdPartyResource::~QnThirdPartyResource()
{
    stopInputPortMonitoring();
}

QnAbstractPtzController* QnThirdPartyResource::getPtzController()
{
    //TODO/IMPL
    return NULL;
}

bool QnThirdPartyResource::isResourceAccessible()
{
    return updateMACAddress();
}

QString QnThirdPartyResource::manufacture() const
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

#if 0
const QnResourceAudioLayout* QnThirdPartyResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    //TODO/IMPL
    return NULL;
    //if (isAudioEnabled()) {
    //    const QnAxisStreamReader* axisReader = dynamic_cast<const QnAxisStreamReader*>(dataProvider);
    //    if (axisReader && axisReader->getDPAudioLayout())
    //        return axisReader->getDPAudioLayout();
    //    else
    //        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    //}
    //else
    //    return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}
#endif

//!Implementation of QnSecurityCamResource::getRelayOutputList
QStringList QnThirdPartyResource::getRelayOutputList() const
{
    //TODO/IMPL
    return QStringList();
}

QStringList QnThirdPartyResource::getInputPortList() const
{
    //TODO/IMPL
    return QStringList();
}

//!Implementation of QnSecurityCamResource::setRelayOutputState
bool QnThirdPartyResource::setRelayOutputState(
    const QString& outputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    //TODO/IMPL
    return false;
}

const QList<nxcip::Resolution>& QnThirdPartyResource::getEncoderResolutionList( int encoderNumber ) const
{
    return m_encoderData[encoderNumber].resolutionList;
}

bool QnThirdPartyResource::initInternal()
{
    m_camManager.setCredentials( getAuth().user(), getAuth().password() );

    int result = m_camManager.getCameraInfo( &m_camInfo );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( QString::fromLatin1("Error getting camera info from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getErrorString(result)), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return false;
    }

    setFirmware( QString::fromUtf8(m_camInfo.firmware) );

    int encoderCount = 0;
    result = m_camManager.getEncoderCount( &encoderCount );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( QString::fromLatin1("Error getting encoder count from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getErrorString(result)), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return false;
    }

    if( encoderCount == 0 )
    {
        NX_LOG( QString::fromLatin1("Third-party camera %1:%2 (url %3) returned 0 encoder count!").arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logDEBUG1 );
        return false;
    }

    //setting camera capabilities
    unsigned int cameraCapabilities = 0;
    result = m_camManager.getCameraCapabilities( &cameraCapabilities );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_LOG( QString::fromLatin1("Error reading camera capabilities from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager.getErrorString(result)), cl_logDEBUG1 );
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? QnResource::Unauthorized : QnResource::Offline );
        return false;
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability )
        setCameraCapability( Qn::RelayInputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability )
        setCameraCapability( Qn::RelayOutputCapability, true );
    if( cameraCapabilities & nxcip::BaseCameraManager::ptzCapability )
    {
        setCameraCapability( Qn::AbsolutePtzCapability, true );
        //TODO/IMPL requesting nxcip::CameraPTZManager interface and setting capabilities
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::audioCapability )
        setAudioEnabled( true );
    //if( cameraCapabilities & nxcip::BaseCameraManager::shareFpsCapability )
    //    setCameraCapability( Qn:: );
    //if( cameraCapabilities & nxcip::BaseCameraManager::sharePixelsCapability )
    //    setCameraCapability( Qn:: );

    m_encoderData.clear();
    m_encoderData.resize( encoderCount );

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
                arg(encoderNumber).arg(m_camManager.getErrorString(result)), cl_logDEBUG1 );
            if( result == nxcip::NX_NOT_AUTHORIZED )
                setStatus( QnResource::Unauthorized );
            return false;
        }
        for( int j = 0; j < resolutionInfoList.size(); ++j )
        {
            m_encoderData[encoderNumber].resolutionList.push_back( resolutionInfoList[j].resolution );
            if( resolutionInfoList[j].maxFps > maxFps )
                maxFps = resolutionInfoList[j].maxFps;
        }
    }

    if( !setParam( MAX_FPS_PARAM_NAME, maxFps, QnDomainDatabase ) )
    {
        NX_LOG( QString::fromLatin1("Failed to set %1 parameter to %2 for third-party camera %3:%4 (url %5)").
            arg(MAX_FPS_PARAM_NAME).arg(maxFps).arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)), cl_logDEBUG1 );
    }

    //TODO/IMPL initializing I/O

    //TODO/IMPL initializing PTZ

    // TODO: #Elric this is totally evil, copypasta from ONVIF resource.
    {
        QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
        if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>()) != 0)
            qnCritical("Can't save resource %1 to Enterprise Controller. Error: %2.", getName(), conn->getLastError());
    }

    return true;
}

bool QnThirdPartyResource::startInputPortMonitoring()
{
    //TODO/IMPL get nxcip::CameraRelayIOManager interface and initialize I/O
    return false;
}

void QnThirdPartyResource::stopInputPortMonitoring()
{
    //TODO/IMPL
}

bool QnThirdPartyResource::isInputPortMonitored() const
{
    //TODO/IMPL
    return false;
}
