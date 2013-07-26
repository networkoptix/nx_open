/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnose_tool.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/http/httptypes.h>


namespace CameraDiagnostics
{
    DiagnoseTool::DiagnoseTool( const QnId& cameraID )
    :
        m_cameraID( cameraID ),
        m_state( sInit ),
        m_step( DiagnosticsStep::mediaServerAvailability ),
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
    DiagnosticsStep::Value DiagnoseTool::currentStep() const
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

    void DiagnoseTool::onCameraDiagnosticsStepResponse( int status, QnCameraDiagnosticsReply reply, int /*handle*/ )
    {
        if( status != nx_http::StatusCode::ok )
        {
            //considering server unavailable
            m_errorMessage = tr("Bad reply from server %1: %2").
                arg(QLatin1String("hz.hz.hz.hz")).arg(QLatin1String(nx_http::StatusCode::toString(static_cast<nx_http::StatusCode::Value>(status))));
            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_result, m_step, m_errorMessage );
            return;
        }

        const DiagnosticsStep::Value performedStep = static_cast<DiagnosticsStep::Value>(reply.performedStep);
        if( m_step == DiagnosticsStep::mediaServerAvailability )
        {
            emit diagnosticsStepResult( m_step, true, QString() );
            m_step = performedStep;
            emit diagnosticsStepStarted( m_step );
        }

        m_result = reply.errorCode == ErrorCode::noError;
        m_errorMessage = ErrorCode::toString(reply.errorCode, reply.errorParams);
        emit diagnosticsStepResult( performedStep, m_result, m_errorMessage );

        const DiagnosticsStep::Value nextStep = static_cast<DiagnosticsStep::Value>(reply.performedStep+1);

        if( !m_result || nextStep == DiagnosticsStep::end )
        {
            m_state = sDone;
            emit diagnosticsDone( m_result, m_step, m_errorMessage );
            return;
        }

        m_step = nextStep;

        m_serverConnection->doCameraDiagnosticsStepAsync( m_cameraID, m_step,
            this, SLOT(onCameraDiagnosticsStepResponse( int, QnCameraDiagnosticsReply, int )) );
        emit diagnosticsStepStarted( m_step );
    }

    void DiagnoseTool::doMediaServerAvailabilityStep()
    {
        emit diagnosticsStepStarted( m_step );

        if( !m_serverConnection )
        {
            m_errorMessage = tr("No connection to media server %1").arg(QLatin1String("hz.hz.hz.hz"));
            m_result = false;
            emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
            m_state = sDone;
            emit diagnosticsDone( m_result, m_step, m_errorMessage );
            return;
        }

        m_serverConnection->doCameraDiagnosticsStepAsync( m_cameraID, static_cast<DiagnosticsStep::Value>(m_step+1),
            this, SLOT(onCameraDiagnosticsStepResponse( int, QnCameraDiagnosticsReply, int )) );
    }
}
