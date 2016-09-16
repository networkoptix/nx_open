/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"
#include "utils/compatibility_version_installation_tool.h"
#include "update/media_server_update_tool.h"
#include "common/common_module.h"


CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(const QnSoftwareVersion &version, QWidget *parent) :
    base_type(parent),
    m_ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(version)
{
    m_ui->setupUi(this);
}

CompatibilityVersionInstallationDialog::~CompatibilityVersionInstallationDialog() {
    if (m_compatibilityTool)
        m_compatibilityTool->cancel();
}

bool CompatibilityVersionInstallationDialog::installationSucceeded() const {
    return m_installationOk;
}

int CompatibilityVersionInstallationDialog::exec() {
    if (useUpdate(m_versionToInstall))
        return installUpdate();
    else
        return installCompatibilityVersion();
}

bool CompatibilityVersionInstallationDialog::useUpdate(const QnSoftwareVersion &version) {
    return version >= QnSoftwareVersion(2, 3);
}

void CompatibilityVersionInstallationDialog::reject() {
    if (m_compatibilityTool && m_compatibilityTool->isRunning()) {
        m_compatibilityTool->cancel();
        return;
    }

    if (m_updateTool && m_updateTool->stage() != QnFullUpdateStage::Init) {
        m_updateTool->cancelUpdate();
        return;
    }

    QDialog::reject();
}

void CompatibilityVersionInstallationDialog::at_compatibilityTool_statusChanged(int status) {
    QDialogButtonBox::StandardButton button = QDialogButtonBox::Close;

    switch (status) {
    case QnCompatibilityVersionInstallationTool::Installing:
        setMessage(tr("Installing version %1").arg(m_versionToInstall.toString(QnSoftwareVersion::MinorFormat)));
        button = QDialogButtonBox::Cancel;
        break;
    case QnCompatibilityVersionInstallationTool::Canceling:
        button = QDialogButtonBox::Cancel;
        break;
    case QnCompatibilityVersionInstallationTool::Success:
        setMessage(tr("Installation completed"));
        m_installationOk = true;
        button = QDialogButtonBox::Ok;
        break;
    case QnCompatibilityVersionInstallationTool::Failed:
        setMessage(tr("Installation failed"));
        break;
    case QnCompatibilityVersionInstallationTool::Canceled:
        setMessage(tr("Installation has been cancelled"));
        break;
    case QnCompatibilityVersionInstallationTool::CancelFailed:
        setMessage(tr("Could not cancel installation"));
        break;
    default:
        break;
    }

    /* Prevent final status jumping */
    if (status != QnCompatibilityVersionInstallationTool::Installing && status != QnCompatibilityVersionInstallationTool::Canceling) {
        disconnect(m_compatibilityTool, NULL, this, NULL);
        m_ui->progressBar->setValue(100);
    }

    m_ui->buttonBox->setStandardButtons(button);
    m_ui->buttonBox->button(button)->setFocus();
    m_ui->buttonBox->button(button)->setEnabled(status != QnCompatibilityVersionInstallationTool::Canceling);
}

void CompatibilityVersionInstallationDialog::at_updateTool_updateFinished(QnUpdateResult result) {
    /* Prevent final status jumping */
    disconnect(m_updateTool, NULL, this, NULL);
    m_ui->progressBar->setValue(100);

    QDialogButtonBox::StandardButton button = QDialogButtonBox::Close;

    switch (result.result) {
    case QnUpdateResult::Successful:
        setMessage(tr("Installation completed"));
        m_installationOk = true;
        button = QDialogButtonBox::Ok;
        break;
    case QnUpdateResult::Cancelled:
        setMessage(tr("Installation has been cancelled"));
        break;
    default:
        setMessage(tr("Installation failed"));
        break;
    }

    m_ui->buttonBox->setStandardButtons(button);
    m_ui->buttonBox->button(button)->setFocus();
}

int CompatibilityVersionInstallationDialog::installCompatibilityVersion() {
    m_installationOk = false;

    m_compatibilityTool.reset(new QnCompatibilityVersionInstallationTool(m_versionToInstall));
    connect(m_compatibilityTool,    &QnCompatibilityVersionInstallationTool::progressChanged,   m_ui->progressBar,  &QProgressBar::setValue);
    connect(m_compatibilityTool,    &QnCompatibilityVersionInstallationTool::statusChanged,     this,               &CompatibilityVersionInstallationDialog::at_compatibilityTool_statusChanged);
    m_compatibilityTool->start();

    int result = base_type::exec();

    m_compatibilityTool.reset();

    return result;
}

int CompatibilityVersionInstallationDialog::installUpdate() {
    m_installationOk = false;

    m_updateTool.reset(new QnMediaServerUpdateTool());
    connect(m_updateTool,   &QnMediaServerUpdateTool::updateFinished,           this,   &CompatibilityVersionInstallationDialog::at_updateTool_updateFinished);
    connect(m_updateTool,   &QnMediaServerUpdateTool::stageProgressChanged,     this,   [this](QnFullUpdateStage stage, int progress) {
        m_ui->progressBar->setValue((static_cast<int>(stage) * 100 + progress) / static_cast<int>(QnFullUpdateStage::Count));
    });

    setMessage(tr("Installing version %1").arg(m_versionToInstall.toString()));
    m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

    m_updateTool->startOnlineClientUpdate(m_versionToInstall);

    int result = base_type::exec();

    m_updateTool.reset();

    return result;
}

void CompatibilityVersionInstallationDialog::setMessage(const QString& message)
{
    m_ui->progressBar->setFormat(lit("%1\t%p%").arg(message));
}
