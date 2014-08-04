#include "ui_camera_diagnostics_dialog.h"
#include "camera_diagnostics_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <utils/common/delete_later.h>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_name.h>

#include <camera/camera_diagnose_tool.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>


QnCameraDiagnosticsDialog::QnCameraDiagnosticsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::CameraDiagnosticsDialog),
    m_tool(NULL),
    m_started(false),
    m_finished(false)
{
    ui->setupUi(this);
    
    QPushButton *copyButton = new QPushButton(tr("Copy to Clipboard"), this);
    ui->buttonBox->addButton(copyButton, QDialogButtonBox::HelpRole);
    connect(copyButton, SIGNAL(clicked()), this, SLOT(at_copyButton_clicked()));

    setHelpTopic(this, Qn::CameraDiagnostics_Help);
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
    connect(m_tool, SIGNAL(diagnosticsStepStarted(CameraDiagnostics::Step::Value)),
            this, SLOT(at_tool_diagnosticsStepStarted(CameraDiagnostics::Step::Value)));
    connect(m_tool, SIGNAL(diagnosticsStepResult(CameraDiagnostics::Step::Value, bool, QString)),
            this, SLOT(at_tool_diagnosticsStepResult(CameraDiagnostics::Step::Value, bool, QString)));
    connect(m_tool, SIGNAL(diagnosticsDone(CameraDiagnostics::Step::Value, bool, QString)),
            this, SLOT(at_tool_diagnosticsDone()));
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
        ui->titleLabel->setText(tr("Diagnostics for camera %1.").arg(getResourceName(m_resource)));
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
    case CameraDiagnostics::Step::mediaServerAvailability:
        return tr("Checking Server availability");
    case CameraDiagnostics::Step::cameraAvailability:
        return tr("Checking that camera is accessible");
    case CameraDiagnostics::Step::mediaStreamAvailability:
        return tr("Checking that camera provides media stream");
    case CameraDiagnostics::Step::mediaStreamIntegrity: 
        return tr("Checking media stream for errors");
    default:
        return QString();
    }
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepStarted(CameraDiagnostics::Step::Value stepType) {
    m_lastLine = diagnosticsStepText(stepType);

    ui->textEdit->append(m_lastLine);
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepResult(CameraDiagnostics::Step::Value, bool result, const QString &errorMessage) {
    QString message;
    QColor color;
    if(result) {
        message = tr("OK");
        color = QColor(128, 255, 128);
    } else {
        message = tr("FAILED: %1").arg(errorMessage);
        color = qnGlobals->errorTextColor();
    }
    
    QString text = QString(lit("<font color=\"%1\">%2</font>")).arg(color.name()).arg(message);
    ui->textEdit->append(text);
    ui->textEdit->append(lit("<br/>"));
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsDone() {
    ui->textEdit->append(tr("Diagnostics finished"));

    m_finished = true;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::at_copyButton_clicked() {
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(ui->textEdit->toPlainText());
}
