/**********************************************************
* 20 apr 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "avigilon_resource.h"

#include <functional>

#include <utils/common/model_functions.h>
#include <utils/common/synctime.h>
#include <utils/common/timermanager.h>


//!Auxiliary structures for parsing camera response
class QnAvigilonInputPortState
{
public:
    QnAvigilonInputPortState( const QString& _currentState = QString() )
    :
        currentState( _currentState )
    {
    }

    QString currentState;
};
#define QnAvigilonInputPortState_Fields (currentState)

class QnAvigilonCheckInputPortResponse
{
public:
    std::vector<QnAvigilonInputPortState> inputs;
};
#define QnAvigilonCheckInputPortResponse_Fields (inputs)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( (QnAvigilonInputPortState)(QnAvigilonCheckInputPortResponse), (json), _Fields )


QnAvigilonResource::QnAvigilonResource()
:
    m_inputMonitored( false ),
    m_checkInputPortStatusTimerID( 0 )
{
    QnAvigilonCheckInputPortResponse inputPortsData;
    inputPortsData.inputs.push_back( lit("Opened") );
    auto str = QJson::serialized( inputPortsData );
}

QnAvigilonResource::~QnAvigilonResource()
{
    stopInputPortMonitoringAsync();
}

static const int INPUT_PORT_CHECK_PERIOD_MS = 1000;

bool QnAvigilonResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        !hasCameraCapabilities(Qn::RelayInputCapability) )
    {
        return false;
    }

    const auto& auth = getAuth();

    m_checkInputUrl = QUrl(getUrl());
    m_checkInputUrl.setScheme( lit("http") );
    m_checkInputUrl.setPath( lit("/cgi-x/get-digitalio") );
    m_checkInputUrl.setUserName( auth.user() );
    m_checkInputUrl.setPassword( auth.password() );

    QMutexLocker lk(&m_ioPortMutex);

    Q_ASSERT( !m_inputMonitored );
    m_inputMonitored = true;

    m_checkInputPortStatusTimerID = TimerManager::instance()->addTimer(
        std::bind(&QnAvigilonResource::checkInputPortState, this, std::placeholders::_1),
        INPUT_PORT_CHECK_PERIOD_MS );
    return true;
}

void QnAvigilonResource::stopInputPortMonitoringAsync()
{
    quint64 checkInputPortStatusTimerID = 0;
    {
        QMutexLocker lk(&m_ioPortMutex);
        m_inputMonitored = false;
        checkInputPortStatusTimerID = m_checkInputPortStatusTimerID;
        m_checkInputPortStatusTimerID = 0;
    }

    if( checkInputPortStatusTimerID > 0 )
        TimerManager::instance()->joinAndDeleteTimer(checkInputPortStatusTimerID);

    m_checkInputPortsRequest.reset();
}

bool QnAvigilonResource::isInputPortMonitored() const
{
    QMutexLocker lk(&m_ioPortMutex);
    return m_inputMonitored;
}

void QnAvigilonResource::checkInputPortState( qint64 timerID )
{
    QMutexLocker lk(&m_ioPortMutex);

    if( timerID != m_checkInputPortStatusTimerID )
        return;

    m_checkInputPortStatusTimerID = 0;
    if( !m_inputMonitored )
        return;

    if( !m_checkInputPortsRequest )
    {
        m_checkInputPortsRequest = nx_http::AsyncHttpClient::create();
        connect( m_checkInputPortsRequest.get(), &nx_http::AsyncHttpClient::done,
                 this, &QnAvigilonResource::onCheckPortRequestDone, Qt::DirectConnection );
    }

    if( !m_checkInputPortsRequest->doGet( m_checkInputUrl ) )
    {
        m_checkInputPortStatusTimerID = TimerManager::instance()->addTimer(
            std::bind(&QnAvigilonResource::checkInputPortState, this, std::placeholders::_1),
            INPUT_PORT_CHECK_PERIOD_MS );
    }
}

void QnAvigilonResource::onCheckPortRequestDone( nx_http::AsyncHttpClientPtr httpClient )
{
    QMutexLocker lk(&m_ioPortMutex);

    if( !m_inputMonitored )
        return;

    if( !httpClient->failed() &&
        httpClient->response()->statusLine.statusCode == nx_http::StatusCode::ok )
    {
        const auto& msgBody = httpClient->fetchMessageBodyBuffer();
        const QnAvigilonCheckInputPortResponse inputPortsData = 
            QJson::deserialized<QnAvigilonCheckInputPortResponse>( msgBody );
    
        if( m_relayInputStates.size() != inputPortsData.inputs.size() )
            m_relayInputStates.resize( inputPortsData.inputs.size() );
        for( size_t i = 0; i < inputPortsData.inputs.size(); ++i )
        {
            const bool isPortClosed = inputPortsData.inputs[i].currentState == lit("Closed");
            if( m_relayInputStates[i] != isPortClosed )
            {
                m_relayInputStates[i] = isPortClosed;
                emit cameraInput(
                    toSharedPointer(),
                    QString::number(i),
                    isPortClosed,
                    qnSyncTime->currentUSecsSinceEpoch() );
            }
        }
    }

    m_checkInputPortStatusTimerID = TimerManager::instance()->addTimer(
        std::bind(&QnAvigilonResource::checkInputPortState, this, std::placeholders::_1),
        INPUT_PORT_CHECK_PERIOD_MS );
}

#endif  //ENABLE_ONVIF
