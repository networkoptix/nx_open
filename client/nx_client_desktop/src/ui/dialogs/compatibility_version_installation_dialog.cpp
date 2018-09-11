#include "compatibility_version_installation_dialog.h"
#include "ui_compatibility_version_installation_dialog.h"

#include <QtWidgets/QPushButton>

#include "update/media_server_update_tool.h"

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog(
    const QnSoftwareVersion& version,
    QWidget* parent) :
    base_type(parent),
    m_ui(new Ui::QnCompatibilityVersionInstallationDialog),
    m_versionToInstall(version)
{
    m_ui->setupUi(this);
}

CompatibilityVersionInstallationDialog::~CompatibilityVersionInstallationDialog()
{
}

bool CompatibilityVersionInstallationDialog::installationSucceeded() const
{
    return m_installationOk;
}

int CompatibilityVersionInstallationDialog::exec()
{
    return installUpdate();
}

void CompatibilityVersionInstallationDialog::reject()
{
    if (m_updateTool && m_updateTool->stage() != QnFullUpdateStage::Init)
    {
        m_updateTool->cancelUpdate();
        return;
    }

    QDialog::reject();
}

void CompatibilityVersionInstallationDialog::at_updateTool_updateFinished(QnUpdateResult result)
{
    /* Prevent final status jumping */
    m_updateTool->disconnect(this);
    m_ui->progressBar->setValue(100);

    QDialogButtonBox::StandardButton button = QDialogButtonBox::Close;

    switch (result.result)
    {
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

int CompatibilityVersionInstallationDialog::installUpdate()
{
    m_updateTool.reset(new QnMediaServerUpdateTool());
    connect(
        m_updateTool,
        &QnMediaServerUpdateTool::updateFinished,
        this,
        &CompatibilityVersionInstallationDialog::at_updateTool_updateFinished);
    connect(
        m_updateTool,
        &QnMediaServerUpdateTool::stageProgressChanged,
        this,
        [this](QnFullUpdateStage stage, int progress)
        {
            m_ui->progressBar->setValue(
                (static_cast<int>(stage) * 100 + progress)
                    / static_cast<int>(QnFullUpdateStage::Count));
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
