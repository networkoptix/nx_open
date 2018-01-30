#include "camera_diagnostics_dialog.h"
#include "ui_camera_diagnostics_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <utils/common/delete_later.h>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <camera/camera_diagnose_tool.h>

#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/client/desktop/ui/common/clipboard_button.h>

using namespace nx::client::desktop::ui;

QnCameraDiagnosticsDialog::QnCameraDiagnosticsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::CameraDiagnosticsDialog),
    m_tool(NULL),
    m_started(false),
    m_finished(false)
{
    ui->setupUi(this);
    retranslateUi();

    auto copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    ui->buttonBox->addButton(copyButton, QDialogButtonBox::HelpRole);
    connect(copyButton, &QPushButton::clicked,
        this, &QnCameraDiagnosticsDialog::at_copyButton_clicked);

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

    retranslateUi();
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

void QnCameraDiagnosticsDialog::retranslateUi()
{

    ui->retranslateUi(this);

    if (!m_resource)
    {
        ui->titleLabel->clear();
        return;
    }

    ui->titleLabel->setText(QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Diagnostics for device %1"),
            tr("Diagnostics for camera %1"),
            tr("Diagnostics for I/O module %1")
        ), m_resource)
    .arg(QnResourceDisplayInfo(m_resource).toString(qnSettings->extraInfoInTree())));


    setWindowTitle(QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device Diagnostics"),
            tr("Camera Diagnostics"),
            tr("I/O Module Diagnostics")
        ) , m_resource));
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
        return QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Confirming device is accessible."),
                tr("Confirming camera is accessible."),
                tr("Confirming I/O module is accessible.")
            ), m_resource);
    case CameraDiagnostics::Step::mediaStreamAvailability:
        return QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Confirming target device provides media stream."),
                tr("Confirming target camera provides media stream."),
                tr("Confirming target I/O module provides media stream.")
            ), m_resource);
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
    ui->textEdit->append(tr("Diagnostics complete"));

    m_finished = true;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::at_copyButton_clicked() {
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(ui->textEdit->toPlainText());
}
