// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_diagnose_tool.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

using namespace nx::vms::client::desktop;

namespace CameraDiagnostics {

namespace {

QString serverHostAddress(const QnVirtualCameraResourcePtr& camera)
{
    if (auto server = camera->getParentServer())
        return QString::fromStdString(server->getPrimaryAddress().address.toString());
    return QString();
}

QString mediaServerUnavailableResult(const QnVirtualCameraResourcePtr& camera)
{
    return MediaServerUnavailableResult(serverHostAddress(camera))
        .toString(camera->resourcePool(), camera);
}

} // namespace

DiagnoseTool::DiagnoseTool(const QnVirtualCameraResourcePtr& camera, QObject* parent):
    QObject(parent),
    SystemContextAware(SystemContext::fromResource(camera)),
    m_camera(camera),
    m_state(sInit),
    m_step(Step::mediaServerAvailability),
    m_result(false)
{
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
    const auto currentSystemId = globalSettings()->localSystemId();
    const bool isConnected = !currentSystemId.isNull();

    if (!success
        || !isConnected
        || (serverSystemId != currentSystemId.toString()))
    {
        m_errorMessage = mediaServerUnavailableResult(m_camera);

        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
        return;
    }

    m_result = true;
    m_errorMessage = ErrorCode::toString(ErrorCode::noError, resourcePool(), m_camera, QList<QString>());

    emit diagnosticsStepResult(m_step, m_result, m_errorMessage);

    Step::Value nextStep = static_cast<Step::Value>(m_step+1);

    doNextStep(nextStep);
}

void DiagnoseTool::onCameraDiagnosticsStepResponse(
    bool success, int /*handle*/, const QnCameraDiagnosticsReply& reply)
{
    if (!success)
    {
        m_errorMessage = mediaServerUnavailableResult(m_camera);

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

    if (!m_result || nextStep == Step::end || !connection())
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

    if (connectedServerApi()->doCameraDiagnosticsStep(
        m_camera->getParentId(),
        m_camera->getId(),
        m_step,
        nx::utils::guarded(this,
            [this](bool success, int handle, const rest::RestResultWithData<QnCameraDiagnosticsReply>& reply)
            {
                onCameraDiagnosticsStepResponse(success, handle, reply.data);
            })) == -1 )
    {
        m_errorMessage = mediaServerUnavailableResult(m_camera);

        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
    }
}

void DiagnoseTool::doMediaServerAvailabilityStep()
{
    emit diagnosticsStepStarted(m_step);

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, QString id)
        {
            onGetServerSystemIdResponse(success, handle, id);
        });

    const auto server = m_camera->getParentServer();
    if (!connection()
        || server->getStatus() != nx::vms::api::ResourceStatus::online
        || connectedServerApi()->getSystemIdFromServer(server->getId(), callback, thread()) == -1)
    {
        m_errorMessage = tr("No connection to Server %1.").arg(serverHostAddress(m_camera));
        m_result = false;
        emit diagnosticsStepResult( m_step, m_result, m_errorMessage );
        m_state = sDone;
        emit diagnosticsDone( m_step, m_result, m_errorMessage );
    }
}

} // namespace CameraDiagnostics
