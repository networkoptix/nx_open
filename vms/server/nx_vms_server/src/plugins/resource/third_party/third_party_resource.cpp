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

#include "api/app_server_connection.h"
#include "nx/streaming/archive_stream_reader.h"
#include <plugins/resource/third_party/motion_data_picture.h>
#include "third_party_archive_delegate.h"
#include "third_party_ptz_controller.h"
#include "third_party_stream_reader.h"
#include "utils/common/synctime.h"

#include <nx/utils/log/log.h>
#include <utils/xml/camera_advanced_param_reader.h>

using namespace nx::sdk;

static const float DEFAULT_MAX_FPS_IN_CASE_IF_UNKNOWN = 30.0;
const QString QnThirdPartyResource::AUX_DATA_PARAM_NAME("aux_data");

static const std::map<Qn::CameraCapability, nxcip::BaseCameraManager::CameraCapability>
    kDeviceCapabilityMap =
{
    {Qn::CameraTimeCapability, nxcip::BaseCameraManager::cameraTimeCapability},
    {Qn::InputPortCapability, nxcip::BaseCameraManager::relayOutputCapability},
    {Qn::OutputPortCapability, nxcip::BaseCameraManager::relayOutputCapability},
    {Qn::ShareIpCapability, nxcip::BaseCameraManager::shareIpCapability},
    {Qn::CustomMediaUrlCapability, nxcip::BaseCameraManager::customMediaUrlCapability},
    {Qn::FixedQualityCapability, nxcip::BaseCameraManager::fixedQualityCapability},
};

static const std::map<Ptz::Capability, nxcip::CameraPtzManager::Capability>
    kPtzCapabilityMap =
{
    {Ptz::ContinuousPanCapability, nxcip::CameraPtzManager::ContinuousPanCapability},
    {Ptz::ContinuousTiltCapability, nxcip::CameraPtzManager::ContinuousTiltCapability},
    {Ptz::ContinuousZoomCapability, nxcip::CameraPtzManager::ContinuousZoomCapability},

    {Ptz::AbsolutePanCapability, nxcip::CameraPtzManager::AbsolutePanCapability},
    {Ptz::AbsoluteTiltCapability, nxcip::CameraPtzManager::AbsoluteTiltCapability},
    {Ptz::AbsoluteZoomCapability, nxcip::CameraPtzManager::AbsoluteZoomCapability},

    {Ptz::FlipPtzCapability, nxcip::CameraPtzManager::FlipPtzCapability},
    {Ptz::LimitsPtzCapability, nxcip::CameraPtzManager::LimitsPtzCapability},
    {Ptz::DevicePositioningPtzCapability, nxcip::CameraPtzManager::DevicePositioningPtzCapability},
    {Ptz::LogicalPositioningPtzCapability, nxcip::CameraPtzManager::LogicalPositioningPtzCapability},
};

QnThirdPartyResource::QnThirdPartyResource(
    QnMediaServerModule* serverModule,
    const nxcip::CameraInfo& camInfo,
    nxcip::BaseCameraManager* camManager,
    const nxcip_qt::CameraDiscoveryManager& discoveryManager )
    :
    nx::vms::server::resource::Camera(serverModule),
    m_camInfo( camInfo ),
    m_camManager( camManager ? new nxcip_qt::BaseCameraManager(camManager) : nullptr ),
    m_discoveryManager( discoveryManager ),
    m_refCounter( 2 ),
    m_encoderCount( 0 ),
    m_cameraManager3( nullptr ),
    m_advancedParametersProvider( this )
{
    setVendor( discoveryManager.getVendorName() );

    if (m_camManager)
    {
        m_cameraManager3 = queryInterfacePtr<nxcip::BaseCameraManager3>(
            m_camManager->getRef(), nxcip::IID_BaseCameraManager3);
    }
}

QnThirdPartyResource::~QnThirdPartyResource()
{
    m_cameraManager3.reset();
    stopInputPortStatesMonitoring();
}

QnAbstractPtzController* QnThirdPartyResource::createPtzControllerInternal() const
{
    if( !m_camManager )
        return nullptr;
    nxcip::CameraPtzManager* ptzManager = m_camManager->getPtzManager();
    if( !ptzManager )
        return NULL;
    return new QnThirdPartyPtzController( toSharedPointer().staticCast<QnThirdPartyResource>(), ptzManager );
}

std::vector<nx::vms::server::resource::Camera::AdvancedParametersProvider*>
    QnThirdPartyResource::advancedParametersProviders()
{
    return {&m_advancedParametersProvider};
}

QnCameraAdvancedParamValueMap QnThirdPartyResource::getApiParameters(const QSet<QString>& ids)
{
    QnMutexLocker lk( &m_mutex );
    if( !m_cameraManager3 )
        return {};

    static const size_t DEFAULT_PARAM_VALUE_BUF_SIZE = 256;
    int valueBufSize = DEFAULT_PARAM_VALUE_BUF_SIZE;
    std::unique_ptr<char[]> valueBuf( new char[valueBufSize] );

    QnCameraAdvancedParamValueMap resultMap;
    for( const auto& id: ids )
    {
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
                    resultMap.insert( id, QString::fromUtf8( valueBuf.get(), valueBufSize ) );
                    i = 3;
                    continue;

                case nxcip::NX_MORE_DATA:
                    valueBuf.reset( new char[valueBufSize] );
                    continue;

                default:
                    break;
            }

            break;
        }
    }

    //TODO #ak return error description
    return {};
}

QSet<QString> QnThirdPartyResource::setApiParameters(const QnCameraAdvancedParamValueMap& values)
{
    if (!setParam("", "{"))
        return QSet<QString>();

    QSet<QString> ids;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        if (setParam(it.key().toUtf8().constData(), it.value().toUtf8().constData()))
            ids.insert(it.key());
    }

    return setParam("", "}") ? ids : QSet<QString>();
}

bool QnThirdPartyResource::setParam(const char * id, const char * value) {
    QnMutexLocker lk( &m_mutex );
    if( !m_cameraManager3 )
        return false;

    return m_cameraManager3->setParamValue( id, value ) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyResource::ping()
{
    // TODO: should check if camera supports http and, if supports, check http port
    return true;
}

static const QString PROPERTIES_TO_MERGE[] = { QnThirdPartyResource::AUX_DATA_PARAM_NAME, ResourcePropertyKey::kFirmware };

bool QnThirdPartyResource::mergeResourcesIfNeeded( const QnNetworkResourcePtr& newResource )
{
    //TODO #ak antipattern: calling virtual function from base class
    bool mergedSomething = base_type::mergeResourcesIfNeeded( newResource );

    // TODO: #ak to make minimal influence on existing code, merging only few properties.
    // But, perharps, other properties should be processed too (in QnResource)
    for( const auto propertyName: PROPERTIES_TO_MERGE )
    {
        const auto newVal = newResource->getProperty( propertyName );
        if( getProperty(propertyName) != newVal )
        {
            setProperty( propertyName, newVal );
            mergedSomething = true;
        }
    }

    return mergedSomething;
}

QString QnThirdPartyResource::getDriverName() const
{
    return m_discoveryManager.getVendorName();
}

QnAbstractStreamDataProvider* QnThirdPartyResource::createLiveDataProvider()
{
    if( !m_camManager )
        return nullptr;
    m_camManager->getRef()->addRef();
    auto result = new ThirdPartyStreamReader(toSharedPointer(this), m_camManager->getRef() );
    unsigned int camCapabilities = 0;
    if (m_camManager->getCameraCapabilities(&camCapabilities) == nxcip::NX_NO_ERROR)
        result->setNeedCorrectTime(camCapabilities & nxcip::BaseCameraManager::relativeTimestampCapability);

    return result;
}

void QnThirdPartyResource::setMotionMaskPhysical(int /*channel*/)
{
    //TODO/IMPL
}

bool QnThirdPartyResource::setOutputPortState(
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
        return nx::vms::server::resource::Camera::createArchiveDataProvider();
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

    const auto camManager2 = queryInterfacePtr<nxcip::BaseCameraManager2>(
        m_camManager->getRef(), nxcip::IID_BaseCameraManager2);
    if( !camManager2 )
        return NULL;

    nxcip::DtsArchiveReader* archiveReader = NULL;
    if( camManager2->createDtsArchiveReader( &archiveReader ) != nxcip::NX_NO_ERROR ||
        archiveReader == NULL )
    {
        return NULL;
    }

    return new ThirdPartyArchiveDelegate( toResourcePtr(), archiveReader );
}

static const unsigned int USEC_IN_MS = 1000;

QnTimePeriodList QnThirdPartyResource::getDtsTimePeriodsByMotionRegion(
    const QList<QRegion>& regions,
    qint64 startTimeMs,
    qint64 endTimeMs,
    int detailLevel,
    bool keepSmalChunks,
    int limit,
    Qt::SortOrder sortOrder)
{
    if( !m_camManager )
        return QnTimePeriodList();

    const auto camManager2 = queryInterfacePtr<nxcip::BaseCameraManager2>(
        m_camManager->getRef(), nxcip::IID_BaseCameraManager2);
    if (!NX_ASSERT(camManager2))
        return QnTimePeriodList();

    nxcip::ArchiveSearchOptions searchOptions;
    if( !regions.isEmpty() )
    {
        //filling in motion mask
        std::unique_ptr<MotionDataPicture> motionDataPicture( new MotionDataPicture( nxcip::AV_PIX_FMT_MONOBLACK ) );

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
                    motionDataPicture->setPixel( x, y, 1 ); // TODO: some optimization would be appropriate
        }

        searchOptions.motionMask = motionDataPicture.release();
    }
    searchOptions.startTime = startTimeMs * USEC_IN_MS;
    searchOptions.endTime = endTimeMs * USEC_IN_MS;
    searchOptions.periodDetailLevel = detailLevel;
    searchOptions.keepSmallChunks = keepSmalChunks;
    searchOptions.limit = limit;
    searchOptions.descSortOrder = (sortOrder == Qt::SortOrder::DescendingOrder);
    nxcip::TimePeriods* timePeriodsRawPtr = NULL;
    const auto findResult = camManager2->find(&searchOptions, &timePeriodsRawPtr);
    const auto timePeriods = toPtr(timePeriodsRawPtr);
    if (findResult != nxcip::NX_NO_ERROR || !timePeriods)
        return QnTimePeriodList();

    QnTimePeriodList timePeriodList;
    for( timePeriods->goToBeginning(); !timePeriods->atEnd(); timePeriods->next() )
    {
        nxcip::UsecUTCTimestamp periodStart = nxcip::INVALID_TIMESTAMP_VALUE;
        nxcip::UsecUTCTimestamp periodEnd = nxcip::INVALID_TIMESTAMP_VALUE;
        timePeriods->get(&periodStart, &periodEnd);

        timePeriodList.push_back(
            QnTimePeriod(periodStart / USEC_IN_MS, (periodEnd - periodStart) / USEC_IN_MS));
    }

    return timePeriodList;
}

QnTimePeriodList QnThirdPartyResource::getDtsTimePeriods(
    qint64 startTimeMs,
    qint64 endTimeMs,
    int detailLevel,
    bool keepSmalChunks,
    int limit,
    Qt::SortOrder sortOrder)
{
    return getDtsTimePeriodsByMotionRegion(
        QList<QRegion>(),
        startTimeMs,
        endTimeMs,
        detailLevel,
        keepSmalChunks,
        limit,
        sortOrder);
}

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

int QnThirdPartyResource::addRef() const
{
    return m_refCounter.fetchAndAddOrdered(1) + 1;
}

int QnThirdPartyResource::releaseRef() const
{
    return m_refCounter.fetchAndAddOrdered(-1) - 1;
}

void QnThirdPartyResource::inputPortStateChanged(
    nxcip::CameraRelayIOManager* /*source*/,
    const char* inputPortID,
    int newState,
    unsigned long int /*timestamp*/ )
{
    emit nx::vms::server::resource::Camera::inputPortStateChanged(
        toSharedPointer(),
        QString::fromUtf8(inputPortID),
        newState != 0,
        qnSyncTime->currentUSecsSinceEpoch() );
}

const QList<nxcip::Resolution>& QnThirdPartyResource::getEncoderResolutionList(StreamIndex encoderNumber) const
{
    QnMutexLocker lk( &m_mutex );
    return m_encoderData[(size_t) encoderNumber].resolutionList;
}

nxcip::Resolution QnThirdPartyResource::getSelectedResolutionForEncoder(StreamIndex encoderIndex) const
{
    QnMutexLocker lk( &m_mutex );
    if( (size_t)encoderIndex < m_selectedEncoderResolutions.size() )
        return m_selectedEncoderResolutions[(size_t) encoderIndex];
    return nxcip::Resolution();
}

CameraDiagnostics::Result QnThirdPartyResource::initializeCameraDriver()
{
    updateDefaultAuthIfEmpty(QString::fromUtf8(m_camInfo.defaultLogin), QString::fromUtf8(m_camInfo.defaultPassword));
    QAuthenticator auth = getAuth();

    if( !m_camManager )
    {
        QnMutexLocker lk( &m_mutex );

        NX_VERBOSE(this, "Restoring camera parameters");
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
            const QByteArray userName = auth.user().toLatin1();
            memset( m_camInfo.defaultLogin, 0, sizeof( m_camInfo.defaultLogin ) );
            strncpy( m_camInfo.defaultLogin, userName.constData(), std::min<size_t>(userName.size(), sizeof(m_camInfo.defaultLogin)-1) );
        }
        if( strlen(m_camInfo.defaultPassword) == 0 )
        {
            const QByteArray userPassword = auth.password().toLatin1();
            memset( m_camInfo.defaultPassword, 0, sizeof( m_camInfo.defaultPassword ) );
            strncpy( m_camInfo.defaultPassword, userPassword.constData(), std::min<size_t>(userPassword.size(), sizeof(m_camInfo.defaultPassword)-1) );
        }

        nxcip::BaseCameraManager* cameraIntf = m_discoveryManager.createCameraManager( m_camInfo );
        if( !cameraIntf )
            return CameraDiagnostics::UnknownErrorResult();
        m_camManager.reset( new nxcip_qt::BaseCameraManager( cameraIntf ) );

        m_cameraManager3 = queryInterfacePtr<nxcip::BaseCameraManager3>(
            m_camManager->getRef(), nxcip::IID_BaseCameraManager3);
    }

    m_camManager->setCredentials(auth.user(), auth.password());

    int result = m_camManager->getCameraInfo( &m_camInfo );
    if (result != nxcip::NX_NO_ERROR)
    {
        NX_DEBUG(this, "Error getting camera info from third-party camera %1:%2 (url %3). %4",
            m_discoveryManager.getVendorName(), QString::fromUtf8(m_camInfo.modelName),
            QString::fromUtf8(m_camInfo.url), m_camManager->getLastErrorString());
        setStatus(result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline);
        return CameraDiagnostics::UnknownErrorResult();
    }

    setFirmware(QString::fromUtf8(m_camInfo.firmware));

    m_encoderCount = 0;
    result = m_camManager->getEncoderCount( &m_encoderCount );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_DEBUG(this, lit("Error getting encoder count from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager->getLastErrorString()));
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline );
        m_encoderCount = 0;
        return CameraDiagnostics::UnknownErrorResult();
    }

    if( m_encoderCount <= 0 )
    {
        NX_DEBUG(this, lit("Third-party camera %1:%2 (url %3) returned 0 encoder count!").arg(m_discoveryManager.getVendorName()).
            arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)));
        m_encoderCount = 0;
        return CameraDiagnostics::UnknownErrorResult();
    }

    //we support only two streams from camera
    m_encoderCount = m_encoderCount > 2 ? 2 : m_encoderCount;

    //setting camera capabilities
    unsigned int cameraCapabilities = 0;
    result = m_camManager->getCameraCapabilities( &cameraCapabilities );
    if( result != nxcip::NX_NO_ERROR )
    {
        NX_DEBUG(this, lit("Error reading camera capabilities from third-party camera %1:%2 (url %3). %4").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
            arg(QString::fromUtf8(m_camInfo.url)).arg(m_camManager->getLastErrorString()));
        setStatus( result == nxcip::NX_NOT_AUTHORIZED ? Qn::Unauthorized : Qn::Offline );
        return CameraDiagnostics::UnknownErrorResult();
    }

    for (const auto& [nxCapabilty, pluginCapability]: kDeviceCapabilityMap)
        setCameraCapability(nxCapabilty, cameraCapabilities & pluginCapability);

    if (cameraCapabilities & nxcip::BaseCameraManager::ptzCapability)
    {
        if (nxcip::CameraPtzManager* ptzManager = m_camManager->getPtzManager())
        {
            const int ptzCapabilities = ptzManager->getCapabilities();
            ptzManager->releaseRef();
            for (const auto& [nxCapabilty, pluginCapability]: kPtzCapabilityMap)
                setPtzCapability(nxCapabilty, ptzCapabilities & pluginCapability);
        }
    }

    bool hasDualStreaming = m_encoderCount > 1;
    if ((cameraCapabilities & nxcip::BaseCameraManager::customMediaUrlCapability)
        && sourceUrl(Qn::CR_SecondaryLiveVideo).isEmpty())
    {
        hasDualStreaming = false;
    }
    setProperty(ResourcePropertyKey::kHasDualStreaming, hasDualStreaming ? 1 : 0);

    setProperty(
        ResourcePropertyKey::kIsAudioSupported,
        (cameraCapabilities & nxcip::BaseCameraManager::audioCapability) ? 1 : 0);
    if( cameraCapabilities & nxcip::BaseCameraManager::dtsArchiveCapability )
    {
        setProperty(ResourcePropertyKey::kDts, 1);
        setProperty(ResourcePropertyKey::kAnalog, 1);
    }
    if( cameraCapabilities & nxcip::BaseCameraManager::hardwareMotionCapability )
    {
        //setMotionType( Qn::MotionType::MT_HardwareGrid );
        setProperty(ResourcePropertyKey::kMotionWindowCnt, 100);
        setProperty(ResourcePropertyKey::kMotionMaskWindowCnt, 100);
        setProperty(ResourcePropertyKey::kMotionSensWindowCnt, 100);
        setProperty(ResourcePropertyKey::kSupportedMotion, QStringLiteral("softwaregrid,hardwaregrid"));
    }
    else
    {
        //setMotionType( Qn::MotionType::MT_SoftwareGrid );
        setProperty(ResourcePropertyKey::kSupportedMotion, QStringLiteral("softwaregrid"));
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
            NX_DEBUG(this, lit("Failed to get resolution list of third-party camera %1:%2 encoder %3. %4").
                arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).
                arg(encoderNumber).arg(m_camManager->getLastErrorString()));
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
        QnMutexLocker lk( &m_mutex );
        m_encoderData = encoderDataTemp;
    }

    if( !maxFps )
        maxFps = DEFAULT_MAX_FPS_IN_CASE_IF_UNKNOWN;

    setMaxFps(maxFps);

    if( (cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability) ||
        (cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability) )
    {
        initializeIOPorts();
    }

    //TODO #ak: current API does not allow to get stream codec, so using AV_CODEC_ID_H264 as true in most cases
    CameraMediaStreams mediaStreams;
    std::vector<nxcip::Resolution> selectedEncoderResolutions(m_encoderCount);
    {
        auto resolution = getMaxResolution(StreamIndex::primary);
        selectedEncoderResolutions[(size_t) StreamIndex::primary] = resolution;
        mediaStreams.streams.push_back(CameraMediaStreamInfo(
            StreamIndex::primary,
            QSize(resolution.width, resolution.height),
            AV_CODEC_ID_H264));
        }
    if((int) StreamIndex::secondary < m_encoderCount )
    {
        auto resolution = getSecondStreamResolution();
        selectedEncoderResolutions[(size_t) StreamIndex::secondary] = resolution;
        mediaStreams.streams.push_back(CameraMediaStreamInfo(
            StreamIndex::secondary,
            QSize(resolution.width, resolution.height),
            AV_CODEC_ID_H264));
    }

    if( m_cameraManager3 )
    {
        //reading camera parameters description (if any)
        const char* paramDescXMLStr = m_cameraManager3->getParametersDescriptionXML();
        if( paramDescXMLStr != nullptr )
        {
            QByteArray paramDescXML = QByteArray::fromRawData( paramDescXMLStr, static_cast<int>(strlen(paramDescXMLStr)) );
            QBuffer dataSource(&paramDescXML);

            if( QnCameraAdvacedParamsXmlParser::validateXml(&dataSource)) {
                //parsing xml to load param list and get cameraId
                QnCameraAdvancedParams params;
                bool success = QnCameraAdvacedParamsXmlParser::readXml(&dataSource, params);

                if (!success) {
                    NX_WARNING(this, lit("Error while parsing xml for the camera (third party) %1 %2 %3")
                        .arg(getVendor())
                        .arg(getModel())
                        .arg(getPhysicalId()));

                    NX_WARNING(this, lit("Faulty xml: %1").arg(QString::fromUtf8(paramDescXML)));
                }

                if (success)
                    m_advancedParametersProvider.assign(std::move(params));
                else
                    m_advancedParametersProvider.clear();
            }
            else {
                NX_WARNING (this, lit("Could not validate camera parameters description xml"));
            }
        }
    }

    {
        QnMutexLocker lk( &m_mutex );
        m_selectedEncoderResolutions = std::move( selectedEncoderResolutions );
    }

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

void QnThirdPartyResource::startInputPortStatesMonitoring()
{
    if (m_relayIOManager.get())
    {
        m_relayIOManager->registerEventHandler(this);
        m_relayIOManager->startInputPortMonitoring();
    }
}

void QnThirdPartyResource::stopInputPortStatesMonitoring()
{
    if (m_relayIOManager.get())
    {
        m_relayIOManager->stopInputPortMonitoring();
        m_relayIOManager->unregisterEventHandler(this);
    }
}

bool QnThirdPartyResource::initializeIOPorts()
{
    //initializing I/O
    nxcip::CameraRelayIOManager* camIOManager = m_camManager->getCameraRelayIOManager();
    if( !camIOManager )
    {
        NX_WARNING(this, lit("Failed to get pointer to nxcip::CameraRelayIOManager interface for third-party camera %1:%2 (url %3)").
            arg(m_discoveryManager.getVendorName()).arg(QString::fromUtf8(m_camInfo.modelName)).arg(QString::fromUtf8(m_camInfo.url)));
        setCameraCapability( Qn::InputPortCapability, false );
        setCameraCapability( Qn::OutputPortCapability, false );
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

nxcip::Resolution QnThirdPartyResource::getMaxResolution(StreamIndex encoderNumber) const
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

nxcip::Resolution QnThirdPartyResource::getNearestResolution(StreamIndex encoderNumber, const nxcip::Resolution& desiredResolution ) const
{
    const QList<nxcip::Resolution>& resolutionList = getEncoderResolutionList( encoderNumber );
    nxcip::Resolution foundResolution;
    for(const nxcip::Resolution& resolution: resolutionList )
    {
        if( resolution.width*resolution.height <= desiredResolution.width*desiredResolution.height &&
            resolution.width*resolution.height > foundResolution.width*foundResolution.height )
        {
            foundResolution = resolution;
        }
    }
    return foundResolution;
}

nxcip::Resolution QnThirdPartyResource::getSecondStreamResolution() const
{
    const nxcip::Resolution& primaryStreamResolution = getMaxResolution(StreamIndex::primary);
    const float currentAspect = nx::vms::server::resource::Camera::getResolutionAspectRatio( QSize(primaryStreamResolution.width, primaryStreamResolution.height) );

    const QList<nxcip::Resolution>& resolutionList = getEncoderResolutionList(StreamIndex::secondary);
    if (resolutionList.isEmpty())
    {
        return nxcip::Resolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION.width(),
            SECONDARY_STREAM_DEFAULT_RESOLUTION.height());
    }

    //preparing data in format suitable for getNearestResolution
    QList<QSize> resList;
    std::transform(
        resolutionList.begin(), resolutionList.end(), std::back_inserter(resList),
        []( const nxcip::Resolution& resolution ){ return QSize(resolution.width, resolution.height); } );

    QSize secondaryResolution = nx::vms::server::resource::Camera::closestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION,
        currentAspect,
        SECONDARY_STREAM_MAX_RESOLUTION,
        resList);

    return nxcip::Resolution( secondaryResolution.width(), secondaryResolution.height() );
}

#endif // ENABLE_THIRD_PARTY
