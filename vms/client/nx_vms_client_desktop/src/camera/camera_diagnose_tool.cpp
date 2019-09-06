/**********************************************************
* 23 jul 2013
* a.kolesnikov
**** *******************************************************/

#include "camera_diagnose_tool.h"

#include <QtNetwork/QNetworkReply>

#include <api/app_server_connection.h>
#include <api/server_rest_connection.h>
#include <api/abstract_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/guarded_callback.h>


namespace CameraDiagnostics
{

DiagnoseTool::DiagnoseTool( const QnUuid& cameraId, QObject *parent )
:
    QObject(parent),
    m_cameraId( cameraId ),
    m_state( sInit ),
    m_step( Step::mediaServerAvailability ),
    m_result( false )
{
    m_camera = resourcePool()->getResourceById<QnVirtualCameraResource>( cameraId );
    if( !m_camera )
        return;

    m_server = m_camera->getParentServer();
    if( !m_server )
        return;
    m_serverHostAddress = m_server->getPrimaryAddress().address.toString();
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

void DiagnoseTool::onGetServerSystemIdResponse(bool success, int /*handle*/, QString serverSystemId)
{
    const ec2::AbstractECConnectionPtr& ecConnection = commonModule()->ec2Connection();
    if( (!success) || !ecConnection || (serverSystemId != ecConnection->connectionInfo().localSystemId.toString()) )
    {
        m_errorMessage = MediaServerUnavailableResult(m_serverHostAddress).toString(resourcePool());

        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
        return;
    }

    m_result = true;
    m_errorMessage = ErrorCode::toString(ErrorCode::noError, resourcePool(), m_camera, QList<QString>());

    Step::Value nextStep = static_cast<Step::Value>(m_step+1);

    doNextStep(nextStep);
}

void DiagnoseTool::onCameraDiagnosticsStepResponse(
    bool success, int /*handle*/, const QnCameraDiagnosticsReply& reply)
{
    if (!success)
    {
        m_errorMessage = MediaServerUnavailableResult(
            m_serverHostAddress).toString(resourcePool());

        m_result = false;
        emit diagnosticsStepResult(m_step, m_result, m_errorMessage);
        m_state = sDone;
        emit diagnosticsDone(m_step, m_result, m_errorMessage);
        return;
    }

    m_result = (reply.errorCode == ErrorCode::noError);
    m_errorMessage = ErrorCode::toString(reply.errorCode, resourcePool(), m_camera, reply.errorParams);
    emit diagnosticsStepResult( static_cast<Step::Value>(reply.performedStep), m_result, m_errorMessage );

    const Step::Value nextStep = static_cast<Step::Value>(reply.performedStep+1);

    if( !m_result || nextStep == Step::end )
    {
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
        return;
    }

    doNextStep(nextStep);
}

void DiagnoseTool::doNextStep(Step::Value nextStep)
{
    m_step = nextStep;
    emit diagnosticsStepStarted( m_step );

    if (m_server->restConnection()->doCameraDiagnosticsStep(m_cameraId, m_step,
        nx::utils::guarded(this,
        [this](bool success, int handle, const rest::RestResultWithData<QnCameraDiagnosticsReply>& reply)
        {
            onCameraDiagnosticsStepResponse(success, handle, reply.data);
        })) == -1 )
    {
        m_errorMessage = MediaServerUnavailableResult(m_serverHostAddress)
            .toString(resourcePool());

        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
    }
}

void DiagnoseTool::doMediaServerAvailabilityStep()
{
    emit diagnosticsStepStarted( m_step );

    if (!m_server || !m_server->restConnection() ||
        m_server->restConnection()->getSystemId(nx::utils::guarded(
            this, [this] (bool success, int handle, QString id)
            {
                onGetServerSystemIdResponse(success, handle, id);
            }), thread()) == -1)
    {
        m_errorMessage = tr("No connection to Server %1.").arg(m_serverHostAddress);
        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
    }
}

} // namespace CameraDiagnostics
