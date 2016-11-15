#include "applauncher_control_dialog.h"

#include <common/common_module.h>

#include <utils/applauncher_utils.h>

nx::client::ui::dialogs::QnApplauncherControlDialog::QnApplauncherControlDialog(QWidget* parent):
    base_type(parent, Qt::Window)
{
    using namespace applauncher;

    auto l = new QVBoxLayout(this);

    {
        auto row = new QHBoxLayout();
        l->addLayout(row);

        auto button = new QPushButton(lit("Check version"), this);
        row->addWidget(button);

        auto edit = new QLineEdit(this);
        row->addWidget(edit, 1);

        auto result = new QLabel(this);
        row->addWidget(result);

        connect(button, &QPushButton::clicked, this, [edit, result]
        {
            QnSoftwareVersion v(edit->text());
            if (v.isNull())
                v = qnCommon->engineVersion();

            bool isInstalled = false;
            auto errCode = isVersionInstalled(v, &isInstalled);

            result->setText(
                lit("Version %1: %2 (%3)")
                .arg(v.toString())
                .arg(isInstalled ? lit("Installed") : lit("Not Installed"))
                .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
            );
        });
    }

    {
        auto row = new QHBoxLayout();
        l->addLayout(row);

        auto button = new QPushButton(lit("Get versions list"), this);
        row->addWidget(button);

        auto result = new QLabel(this);
        row->addWidget(result);

        connect(button, &QPushButton::clicked, this, [result]
        {
            QList<QnSoftwareVersion> versions;
            auto errCode = getInstalledVersions(&versions);

            QStringList text;
            for (auto v : versions)
                text << v.toString();

            result->setText(
                lit("Result %1:\n %2")
                .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
                .arg(text.join(L'\n'))
            );
        });
    }

    l->addStretch();
    setMinimumHeight(500);
}
