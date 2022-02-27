// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "applauncher_control_dialog.h"
#include "ui_applauncher_control_dialog.h"

#include <nx/vms/api/data/software_version.h>
#include <ui/workbench/workbench_context.h>
#include <utils/applauncher_utils.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

ApplauncherControlDialog::ApplauncherControlDialog(QWidget* parent):
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

void ApplauncherControlDialog::registerAction()
{
    registerDebugAction(
        "Applauncher",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<ApplauncherControlDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
