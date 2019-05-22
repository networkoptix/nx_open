#include "applauncher_control_dialog.h"
#include "ui_applauncher_control_dialog.h"

#include <common/static_common_module.h>
#include <utils/applauncher_utils.h>

namespace nx::vms::client::desktop::ui {
QnApplauncherControlDialog::QnApplauncherControlDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    ui(new Ui::ApplauncherControlDialog)
{
    ui->setupUi(this);

    using namespace nx::vms::applauncher::api;

    connect(ui->checkVersionButton, &QPushButton::clicked, this,
        [this]
        {
            nx::utils::SoftwareVersion v(ui->checkVersionlineEdit->text());
            bool isInstalled = false;
            const ResultType errorCode = isVersionInstalled(v, &isInstalled);

            ui->checkVersionLabel->setText(
                QString("Version %1: %2 (%3)").arg(
                    v.toString(),
                    isInstalled ? "Installed" : "Not Installed",
                    toString(errorCode)));
        });

    connect(ui->getVersionsButton, &QPushButton::clicked, this,
        [this]
        {
            QList<nx::utils::SoftwareVersion> versions;
            const ResultType errorCode = getInstalledVersions(&versions);

            QStringList text;
            for (auto v : versions)
                text << v.toString();

            ui->getVersionsLabel->setText(
                QString("Result %1:\n %2").arg(toString(errorCode), text.join(L'\n')));
        });
}

} // namespace nx::vms::client::desktop::ui
