// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_diagnostics_dialog.h"
#include "ui_camera_diagnostics_dialog.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <camera/camera_diagnose_tool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <utils/common/delete_later.h>

using namespace nx::vms::client::desktop;
using DiagnoseTool = CameraDiagnostics::DiagnoseTool;

QnCameraDiagnosticsDialog::QnCameraDiagnosticsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::CameraDiagnosticsDialog),
    m_tool(nullptr),
    m_started(false),
    m_finished(false)
{
    ui->setupUi(this);
    retranslateUi();

    auto copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    ui->buttonBox->addButton(copyButton, QDialogButtonBox::HelpRole);
    connect(copyButton, &QPushButton::clicked,
        this, &QnCameraDiagnosticsDialog::at_copyButton_clicked);

    setHelpTopic(this, HelpTopic::Id::CameraDiagnostics);
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

    m_tool = new CameraDiagnostics::DiagnoseTool(m_resource, this);
    connect(m_tool, &DiagnoseTool::diagnosticsStepStarted,
            this, &QnCameraDiagnosticsDialog::at_tool_diagnosticsStepStarted);
    connect(m_tool, &DiagnoseTool::diagnosticsStepResult,
            this, &QnCameraDiagnosticsDialog::at_tool_diagnosticsStepResult);
    connect(m_tool, &DiagnoseTool::diagnosticsDone,
            this, &QnCameraDiagnosticsDialog::at_tool_diagnosticsDone);
    m_tool->start();

    m_started = true;
    m_finished = false;

    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::stop() {
    if(!m_started)
        return;

    disconnect(m_tool, nullptr, this, nullptr);
    qnDeleteLater(m_tool);
    m_tool = nullptr;

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

    const auto resourcePool = SystemContext::fromResource(m_resource)->resourcePool();

    ui->titleLabel->setText(
        nx::format(
            QnDeviceDependentStrings::getNameFromSet(
                resourcePool,
                QnCameraDeviceStringSet(
                    tr("Diagnostics for device %1"),
                    tr("Diagnostics for camera %1"),
                    tr("Diagnostics for I/O module %1")
                ),
                m_resource),
            QnResourceDisplayInfo(m_resource).toString(
                appContext()->localSettings()->resourceInfoLevel())));


    setWindowTitle(QnDeviceDependentStrings::getNameFromSet(
        resourcePool,
        QnCameraDeviceStringSet(
            tr("Device Diagnostics"),
            tr("Camera Diagnostics"),
            tr("I/O Module Diagnostics")
        ) , m_resource));
}

void QnCameraDiagnosticsDialog::updateOkButtonEnabled()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_started || m_finished);
}

void QnCameraDiagnosticsDialog::clearLog()
{
    ui->textEdit->clear();
}

QString QnCameraDiagnosticsDialog::diagnosticsStepText(int stepType)
{
    const auto resourcePool = SystemContext::fromResource(m_resource)->resourcePool();

    switch(stepType)
    {
        case CameraDiagnostics::Step::mediaServerAvailability:
            return tr("Confirming server availability.");
        case CameraDiagnostics::Step::cameraAvailability:
            return QnDeviceDependentStrings::getNameFromSet(
                resourcePool,
                QnCameraDeviceStringSet(
                    tr("Confirming device is accessible."),
                    tr("Confirming camera is accessible."),
                    tr("Confirming I/O module is accessible.")
                ), m_resource);
        case CameraDiagnostics::Step::mediaStreamAvailability:
            return QnDeviceDependentStrings::getNameFromSet(
                resourcePool,
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

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepStarted(int stepType)
{
    m_lastLine = diagnosticsStepText(stepType);
    ui->textEdit->append(m_lastLine);
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsStepResult(
    int /*step*/, bool result, const QString &errorMessage)
{
    using namespace nx::vms::common;

    QString message;
    QColor color;
    if (result)
    {
        message = tr("OK");
        color = nx::vms::client::core::colorTheme()->color("green_core");
    }
    else
    {
        message = tr("FAILED: %1").arg(errorMessage);
        color = nx::vms::client::core::colorTheme()->color("red_l2");
    }

    ui->textEdit->append(html::colored(message, color));
    ui->textEdit->append(html::kLineBreak);
}

void QnCameraDiagnosticsDialog::at_tool_diagnosticsDone()
{
    ui->textEdit->append(tr("Diagnostics complete"));
    m_finished = true;
    updateOkButtonEnabled();
}

void QnCameraDiagnosticsDialog::at_copyButton_clicked()
{
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(ui->textEdit->toPlainText());
}
