/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"
#include "utils/compatibility_version_installation_tool.h"
#include "common/common_module.h"


QnCompatibilityVersionInstallationDialog::QnCompatibilityVersionInstallationDialog(const QnSoftwareVersion &version, QWidget *parent) :
    base_type(parent),
    m_ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(version)
{
    m_ui->setupUi(this);
}

QnCompatibilityVersionInstallationDialog::~QnCompatibilityVersionInstallationDialog() {
    if (m_compatibilityTool)
        m_compatibilityTool->cancel();
}

bool QnCompatibilityVersionInstallationDialog::installationSucceeded() const {
    return m_installationOk;
}

int QnCompatibilityVersionInstallationDialog::exec() {
    if (m_versionToInstall < qnCommon->engineVersion())
        return installCompatibilityVersion();
    else
        return installUpdate();
}

void QnCompatibilityVersionInstallationDialog::reject() {
    if (m_compatibilityTool && m_compatibilityTool->isRunning()) {
        m_compatibilityTool->cancel();
        return;
    }

    QDialog::reject();
}

void QnCompatibilityVersionInstallationDialog::at_compatibilityTool_statusChanged(int status) {
    bool progress = false;

    switch (status) {
    case QnCompatibilityVersionInstallationTool::Installing:
        setWindowTitle(tr("Installing version %1").arg(m_versionToInstall.toString(QnSoftwareVersion::MinorFormat)));
        progress = true;
        break;
    case QnCompatibilityVersionInstallationTool::Canceling:
        progress = true;
        break;
    case QnCompatibilityVersionInstallationTool::Success:
        setWindowTitle(tr("Installation completed"));
        m_installationOk = true;
        break;
    case QnCompatibilityVersionInstallationTool::Failed:
        setWindowTitle(tr("Installation failed"));
        break;
    case QnCompatibilityVersionInstallationTool::Canceled:
        setWindowTitle(tr("Installation has been cancelled"));
        break;
    case QnCompatibilityVersionInstallationTool::CancelFailed:
        setWindowTitle(tr("Could not cancel installation"));
        break;
    default:
        break;
    }

    if (progress) {
        m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
        m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(
                    status == QnCompatibilityVersionInstallationTool::Installing);
    } else {
        m_ui->buttonBox->setStandardButtons(
                    status == QnCompatibilityVersionInstallationTool::Success ?
                        QDialogButtonBox::Ok : QDialogButtonBox::Close);
    }
}

int QnCompatibilityVersionInstallationDialog::installCompatibilityVersion() {
    m_installationOk = false;

    m_compatibilityTool.reset(new QnCompatibilityVersionInstallationTool(m_versionToInstall));
    connect(m_compatibilityTool,    &QnCompatibilityVersionInstallationTool::progressChanged,   m_ui->progressBar,  &QProgressBar::setValue);
    connect(m_compatibilityTool,    &QnCompatibilityVersionInstallationTool::statusChanged,     this,               &QnCompatibilityVersionInstallationDialog::at_compatibilityTool_statusChanged);
    m_compatibilityTool->start();

    int result = base_type::exec();

    m_compatibilityTool.reset();

    return result;
}

int QnCompatibilityVersionInstallationDialog::installUpdate() {
    //TODO: #dklychkov implement
    return QDialog::Rejected;
}
