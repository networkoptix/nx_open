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
    m_finished(false),
    m_targetDevice(),
    m_upperCaseDevice()
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

    updateTexts();
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

void QnCameraDiagnosticsDialog::updateTexts() {
    
    if(!m_resource)
    {
        ui->titleLabel->clear();
        return;
    } 
    
    //: %1 - will be substituted by type of device ("camera", "io module", etc..); %2 - will be substituted by model of device; Example: "Diagnostics for camera X1323"
    const QString titleLabelText = tr("Diagnostics for %1 %2.");
    ui->titleLabel->setText(titleLabelText.arg(getDevicesName(QnVirtualCameraResourceList() << m_resource, false)).arg(getResourceName(m_resource)));

    //: %1 - will be substituted by type of device ("Camera", "IO Module", etc..); Example: "IO Module Diagnostics"
    setWindowTitle(tr("%1 Diagnostics").arg(getDevicesName(QnVirtualCameraResourceList() << m_resource)));
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
        return tr("Confirming server availability.");
    case CameraDiagnostics::Step::cameraAvailability:
        return tr("Confirming %1 is accessible.").arg(getDevicesName(QnVirtualCameraResourceList() << m_resource, false));
    case CameraDiagnostics::Step::mediaStreamAvailability:
        return tr("Confirming target %1 provides media stream.").arg(getDevicesName(QnVirtualCameraResourceList() << m_resource, false));
    case CameraDiagnostics::Step::mediaStreamIntegrity: 
        return tr("Evaluating media stream for errors.");
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
    ui->textEdit->append(tr("Diagnostics complete!"));

    m_finished = true;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::at_copyButton_clicked() {
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(ui->textEdit->toPlainText());
}
