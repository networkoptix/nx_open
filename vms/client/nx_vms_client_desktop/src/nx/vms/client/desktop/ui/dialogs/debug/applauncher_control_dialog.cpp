#include "applauncher_control_dialog.h"
#include "ui_applauncher_control_dialog.h"

#include <common/static_common_module.h>

#include <utils/applauncher_utils.h>

namespace nx::vms::client::desktop {
namespace ui {

QnApplauncherControlDialog::QnApplauncherControlDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    ui(new Ui::ApplauncherControlDialog)
{
    ui->setupUi(this);

    using namespace applauncher::api;

    connect(ui->checkVersionButton, &QPushButton::clicked, this,
        [this]
        {
            nx::utils::SoftwareVersion v(ui->checkVersionlineEdit->text());
            if (v.isNull())
                v = qnStaticCommon->engineVersion();

            bool isInstalled = false;
            auto errCode = isVersionInstalled(v, &isInstalled);

            ui->checkVersionLabel->setText(
                lit("Version %1: %2 (%3)")
                .arg(v.toString())
                .arg(isInstalled ? lit("Installed") : lit("Not Installed"))
                .arg(QString::fromUtf8(ResultType::toString(errCode)))
            );
        });

    connect(ui->getVersionsButton, &QPushButton::clicked, this,
        [this]
        {
            QList<nx::utils::SoftwareVersion> versions;
            auto errCode = getInstalledVersions(&versions);

            QStringList text;
            for (auto v : versions)
                text << v.toString();

            ui->getVersionsLabel->setText(
                lit("Result %1:\n %2")
                .arg(QString::fromUtf8(ResultType::toString(errCode)))
                .arg(text.join(L'\n'))
            );
        });

}

} // namespace ui
} // namespace nx::vms::client::desktop
