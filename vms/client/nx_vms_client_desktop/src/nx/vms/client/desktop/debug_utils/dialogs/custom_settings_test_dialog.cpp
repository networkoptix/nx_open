// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_settings_test_dialog.h"
#include "ui_custom_settings_test_dialog.h"

#include <QtCore/QBuffer>

#include <nx/fusion/model_functions.h>
#include <ui/workbench/workbench_context.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

CustomSettingsTestDialog::CustomSettingsTestDialog(QWidget* parent /*= nullptr*/):
    base_type(parent, Qt::Window),
    ui(new Ui::CustomSettingsTestDialog())
{
    ui->setupUi(this);

    connect(ui->loadButton, &QPushButton::clicked, this,
        &CustomSettingsTestDialog::loadManifest);
}

CustomSettingsTestDialog::~CustomSettingsTestDialog()
{
}

void CustomSettingsTestDialog::loadManifest()
{
    const QString serializedManifest = ui->manifestTextEdit->toPlainText().trimmed();
    QnCameraAdvancedParams manifest;

    // Naive detect was it xml or json.
    if (serializedManifest.startsWith('<'))
    {
        QBuffer reader;
        reader.setData(serializedManifest.toUtf8());
        QnCameraAdvancedParamsXmlParser::readXml(&reader, manifest);
    }
    else
    {
        QJson::deserialize<QnCameraAdvancedParams>(serializedManifest, &manifest);
    }
    ui->settingsWidget->loadManifest(manifest);
}

void CustomSettingsTestDialog::registerAction()
{
    registerDebugAction(
        "Custom Settings",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<CustomSettingsTestDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
