/**********************************************************
* 20 apr 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "avigilon_resource.h"

#include <functional>

#include <nx/fusion/model_functions.h>
#include <utils/common/synctime.h>
#include <nx/utils/timer_manager.h>


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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnAvigilonInputPortState)(QnAvigilonCheckInputPortResponse),
    (json),
    _Fields,
    (optional, false))

QnAvigilonResource::QnAvigilonResource(QnMediaServerModule* serverModule)
    :
    QnPlOnvifResource(serverModule),
    m_inputMonitored( false ),
    m_checkInputPortStatusTimerID( 0 )
{
    QnAvigilonCheckInputPortResponse inputPortsData;
    inputPortsData.inputs.push_back( lit("Opened") );
    auto str = QJson::serialized( inputPortsData );
}

QnAvigilonResource::~QnAvigilonResource()
{
    stopInputPortStatesMonitoring();
}

static const int INPUT_PORT_CHECK_PERIOD_MS = 1000;

void QnAvigilonResource::startInputPortStatesMonitoring()
{
    m_checkInputUrl = nx::utils::Url(getUrl());
    m_checkInputUrl.setScheme( lit("http") );
    m_checkInputUrl.setPath( lit("/cgi-x/get-digitalio") );

    QAuthenticator auth = getAuth();

    m_checkInputUrl.setUserName( auth.user() );
    m_checkInputUrl.setPassword( auth.password() );

    QnMutexLocker lk(&m_ioPortMutex);

    NX_ASSERT( !m_inputMonitored );
    m_inputMonitored = true;

    m_checkInputPortStatusTimerID = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&QnAvigilonResource::checkInputPortState, this, std::placeholders::_1),
        std::chrono::milliseconds(INPUT_PORT_CHECK_PERIOD_MS));
}

void QnAvigilonResource::stopInputPortStatesMonitoring()
{
    quint64 checkInputPortStatusTimerID = 0;
    {
        QnMutexLocker lk(&m_ioPortMutex);
        m_inputMonitored = false;
        checkInputPortStatusTimerID = m_checkInputPortStatusTimerID;
        m_checkInputPortStatusTimerID = 0;
    }

    if( checkInputPortStatusTimerID > 0 )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(checkInputPortStatusTimerID);

    m_checkInputPortsRequest.reset();
}

void QnAvigilonResource::checkInputPortState( qint64 timerID )
{
    QnMutexLocker lk(&m_ioPortMutex);

    if( timerID != m_checkInputPortStatusTimerID )
        return;

    m_checkInputPortStatusTimerID = 0;
    if( !m_inputMonitored )
        return;

    if( !m_checkInputPortsRequest )
    {
        m_checkInputPortsRequest = nx::network::http::AsyncHttpClient::create();
        connect( m_checkInputPortsRequest.get(), &nx::network::http::AsyncHttpClient::done,
                 this, &QnAvigilonResource::onCheckPortRequestDone, Qt::DirectConnection );
    }

    m_checkInputPortsRequest->doGet(m_checkInputUrl);
}

void QnAvigilonResource::onCheckPortRequestDone( nx::network::http::AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk(&m_ioPortMutex);

    if( !m_inputMonitored )
        return;

    if( !httpClient->failed() &&
        httpClient->response()->statusLine.statusCode == nx::network::http::StatusCode::ok )
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
                emit inputPortStateChanged(
                    toSharedPointer(),
                    QString::number(i),
                    isPortClosed,
                    qnSyncTime->currentUSecsSinceEpoch() );
            }
        }
    }

    m_checkInputPortStatusTimerID = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&QnAvigilonResource::checkInputPortState, this, std::placeholders::_1),
        std::chrono::milliseconds(INPUT_PORT_CHECK_PERIOD_MS));
}

#endif  //ENABLE_ONVIF
