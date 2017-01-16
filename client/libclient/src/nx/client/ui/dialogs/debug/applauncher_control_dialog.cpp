#include "applauncher_control_dialog.h"
#include "ui_applauncher_control_dialog.h"

#include <common/common_module.h>

#include <utils/applauncher_utils.h>

nx::client::ui::dialogs::QnApplauncherControlDialog::QnApplauncherControlDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    ui(new Ui::ApplauncherControlDialog)
{
    ui->setupUi(this);

    using namespace applauncher;

    connect(ui->checkVersionButton, &QPushButton::clicked, this,
        [this]
        {
            QnSoftwareVersion v(ui->checkVersionlineEdit->text());
            if (v.isNull())
                v = qnCommon->engineVersion();

            bool isInstalled = false;
            auto errCode = isVersionInstalled(v, &isInstalled);

            ui->checkVersionLabel->setText(
                lit("Version %1: %2 (%3)")
                .arg(v.toString())
                .arg(isInstalled ? lit("Installed") : lit("Not Installed"))
                .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
            );
        });

    connect(ui->getVersionsButton, &QPushButton::clicked, this,
        [this]
        {
            QList<QnSoftwareVersion> versions;
            auto errCode = getInstalledVersions(&versions);

            QStringList text;
            for (auto v : versions)
                text << v.toString();

            ui->getVersionsLabel->setText(
                lit("Result %1:\n %2")
                .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
                .arg(text.join(L'\n'))
            );
        });

}
