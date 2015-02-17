/**********************************************************
* 23 jul 2013
* a.kolesnikov
**** *******************************************************/

#include "camera_diagnose_tool.h"

#include <QtNetwork/QNetworkReply>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/http/httptypes.h>


namespace CameraDiagnostics
{
    DiagnoseTool::DiagnoseTool( const QnUuid& cameraID, QObject *parent )
    :
        QObject(parent),
        m_cameraID( cameraID ),
        m_state( sInit ),
        m_step( Step::mediaServerAvailability ),
        m_result( false )
    {
        QnResourcePtr resource = QnResourcePool::instance()->getResourceById( cameraID );
        if( !resource )
            return;
        QnSecurityCamResourcePtr secCamRes = resource.dynamicCast<QnSecurityCamResource>();
        if( !secCamRes )
            return;

        QnMediaServerResourcePtr serverResource = qSharedPointerDynamicCast<QnMediaServerResource>(QnResourcePool::instance()->getResourceById(resource->getParentId()));
        if( !serverResource )
            return;
        m_serverConnection = serverResource->apiConnection();
        m_serverHostAddress = QUrl(serverResource->getApiUrl()).host();
    }

    DiagnoseTool::~DiagnoseTool()
    {
    }

    //!Starts diagnostics and returns immediately
    void DiagnoseTool::start()
    {
        if( m_state > sInit )
            return;

        m_state = sInProgress;

        doMediaServerAvailabilityStep();
    }

    //!Returns object's current state
    DiagnoseTool::State DiagnoseTool::state() const
    {
        return m_state;
    }

    //!Valid only after \a DiagnoseTool::start call. After diagnostics is finished returns final step
    Step::Value DiagnoseTool::currentStep() const
    {
        return m_step;
    }

    //!Valid only after diagnostics finished
    bool DiagnoseTool::result() const
    {
        return m_result;
    }

    //!Valid only after diagnostics finished
    QString DiagnoseTool::errorMessage() const
    {
        return m_errorMessage;
    }

    void DiagnoseTool::onGetServerSystemNameResponse( int status, QString serverSystemName, int /*handle*/ )
    {
        const ec2::AbstractECConnectionPtr& ecConnection = QnAppServerConnectionFactory::getConnection2();
        if( (status != 0) || !ecConnection || (serverSystemName != ecConnection->connectionInfo().systemName) )
        {
            m_errorMessage = CameraDiagnostics::MediaServerUnavailableResult(m_serverHostAddress).toString();

            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
            return;
        }

        m_result = true;
        m_errorMessage = ErrorCode::toString(ErrorCode::noError, QList<QString>());
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );

        emit diagnosticsStepStarted( static_cast<Step::Value>(m_step+1) );
        if( m_serverConnection->doCameraDiagnosticsStepAsync(
                m_cameraID,
                static_cast<Step::Value>(m_step+1),
                this,
                SLOT(onCameraDiagnosticsStepResponse( int, QnCameraDiagnosticsReply, int )) ) == -1 )
        {
            m_errorMessage = CameraDiagnostics::MediaServerUnavailableResult(m_serverHostAddress).toString();

            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
        }
    }

    void DiagnoseTool::onCameraDiagnosticsStepResponse( int status, QnCameraDiagnosticsReply reply, int /*handle*/ )
    {
        if( status != 0 )
        {
            switch( status )
            {
                case QNetworkReply::ContentAccessDenied:
                case QNetworkReply::AuthenticationRequiredError:
                    m_errorMessage = CameraDiagnostics::MediaServerBadResponseResult(m_serverHostAddress, QLatin1String("Unauthorized")).toString();    //401
                    break;
                case QNetworkReply::ContentOperationNotPermittedError:
                    m_errorMessage = CameraDiagnostics::MediaServerBadResponseResult(m_serverHostAddress, QLatin1String("Forbidden")).toString();       //403
                    break;
                case QNetworkReply::ContentNotFoundError:
                    m_errorMessage = CameraDiagnostics::MediaServerBadResponseResult(m_serverHostAddress, QLatin1String("Not Found")).toString();       //404
                    break;
                case QNetworkReply::ProtocolFailure:
                    m_errorMessage = CameraDiagnostics::MediaServerBadResponseResult(m_serverHostAddress, QString()).toString();
                    break;

                default:
                    m_errorMessage = CameraDiagnostics::MediaServerUnavailableResult(m_serverHostAddress).toString();
            }

            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
            return;
        }

        m_result = reply.errorCode == ErrorCode::noError;
        m_errorMessage = ErrorCode::toString(reply.errorCode, reply.errorParams);
        emit diagnosticsStepResult( static_cast<Step::Value>(reply.performedStep), m_result, m_errorMessage );

        const Step::Value nextStep = static_cast<Step::Value>(reply.performedStep+1);

        if( !m_result || nextStep == Step::end )
        {
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
            return;
        }

        m_step = nextStep;
        emit diagnosticsStepStarted( m_step );

        if( m_serverConnection->doCameraDiagnosticsStepAsync(
                m_cameraID,
                m_step,
                this,
                SLOT(onCameraDiagnosticsStepResponse( int, QnCameraDiagnosticsReply, int )) ) == -1 )
        {
            m_errorMessage = CameraDiagnostics::MediaServerUnavailableResult(m_serverHostAddress).toString();

            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
        }
    }

    void DiagnoseTool::doMediaServerAvailabilityStep()
    {
        emit diagnosticsStepStarted( m_step );

        if( !m_serverConnection ||
            m_serverConnection->getSystemNameAsync(
                this,
                SLOT(onGetServerSystemNameResponse(int, QString, int)) ) == -1 )
        {
            m_errorMessage = tr("No connection to Server %1.").arg(m_serverHostAddress);
            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_step, m_result, m_errorMessage );
        }
    }
}
