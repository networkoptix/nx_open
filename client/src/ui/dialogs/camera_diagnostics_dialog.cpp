#include "ui_camera_diagnostics_dialog.h"
#include "camera_diagnostics_dialog.h"

#include <utils/common/delete_later.h>

#include <core/resource/camera_resource.h>

#include <camera/camera_diagnose_tool.h>
#include <ui/style/globals.h>

QnCameraDiagnosticsDialog::QnCameraDiagnosticsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::CameraDiagnosticsDialog),
    m_tool(NULL),
    m_started(false),
    m_finished(false)
{
    ui->setupUi(this);
}

QnCameraDiagnosticsDialog::~QnCameraDiagnosticsDialog() {
    stop();
}

void QnCameraDiagnosticsDialog::setResource(const QnVirtualCameraResourcePtr &resource) {
    if(m_resource == resource)
        return;

    stop();

    m_resource = resource;

    updateTitleText();
    clearLog();
}

QnVirtualCameraResourcePtr QnCameraDiagnosticsDialog::resource() {
    return m_resource;
}

void QnCameraDiagnosticsDialog::restart() {
    if(!m_resource)
        return;

    if(m_started)
        clearLog();

    stop();

    m_tool = new CameraDiagnostics::DiagnoseTool(m_resource->getId(), this);
    connect(m_tool, SIGNAL(diagnosticsStepStarted(int)),                this, SLOT(at_tool_diagnosticsStepStarted(int)));
    connect(m_tool, SIGNAL(diagnosticsStepResult(int, bool, QString)),  this, SLOT(at_tool_diagnosticsStepResult(int, bool, QString)));
    connect(m_tool, SIGNAL(diagnosticsDone(int, bool, QString)),        this, SLOT(at_tool_diagnosticsDone(int, bool, QString)));
    m_tool->start();
    
    m_started = true;
    m_finished = false;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::stop() {
    if(!m_started)
        return;

    disconnect(m_tool, NULL, this, NULL);
    qnDeleteLater(m_tool);
    m_tool = NULL;

    m_started = false;
    m_finished = false;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::updateTitleText() {
    if(!m_resource) {
        ui->titleLabel->clear();
    } else {
        ui->titleLabel->setText(tr("Diagnostics for camera %1.").arg(m_resource->getName()));
    }
}

void QnCameraDiagnosticsDialog::updateOkButtonEnabled() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_started || m_finished);
}

void QnCameraDiagnosticsDialog::clearLog() {
    ui->textEdit->clear();
}

QString QnCameraDiagnosticsDialog::diagnosticsStepText(int stepType) {
    switch(stepType) {
    case CameraDiagnostics::DiagnosticsStep::mediaServerAvailability:
        return tr("Checking media server availability");
    case CameraDiagnostics::DiagnosticsStep::cameraAvailability:
        return tr("Checking that camera responses on base API requests");
    case CameraDiagnostics::DiagnosticsStep::mediaStreamAvailability:
        return tr("Checking that camera provides media stream");
    case CameraDiagnostics::DiagnosticsStep::mediaStreamIntegrity: 
        return tr("Checking media stream provided by camera for errors");
    default:
        return QString();
    }
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepStarted(int stepType) {
    m_lastLine = diagnosticsStepText(stepType);

    ui->textEdit->append(m_lastLine);
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepResult(int stepType, bool result, const QString &errorMessage) {
    QString message;
    QColor color;
    if(result) {
        message = tr("OK");
        color = QColor(128, 255, 128);
    } else {
        message = tr("FAILED");
        color = qnGlobals->errorTextColor();
    }
    
    QString text = QString(lit("<font color=\"%1\">%2</font>")).arg(color.name()).arg(message);
    ui->textEdit->append(text);
    ui->textEdit->append(lit("<br/>"));
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsDone(int finalStep, bool result, const QString &errorMessage) {
    ui->textEdit->append(tr("Diagnostics finished"));

    m_finished = true;

    updateOkButtonEnabled();
}

