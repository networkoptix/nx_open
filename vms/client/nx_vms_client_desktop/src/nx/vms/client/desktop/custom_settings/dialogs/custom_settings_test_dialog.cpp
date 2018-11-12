#include "custom_settings_test_dialog.h"
#include "ui_custom_settings_test_dialog.h"

#include <QtCore/QBuffer>

#include <utils/xml/camera_advanced_param_reader.h>

#include <nx/fusion/model_functions.h>

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
    if (serializedManifest.startsWith(L'<'))
    {
        QBuffer reader;
        reader.setData(serializedManifest.toUtf8());
        QnCameraAdvacedParamsXmlParser::readXml(&reader, manifest);
    }
    else
    {
        QJson::deserialize<QnCameraAdvancedParams>(serializedManifest, &manifest);
    }
    ui->settingsWidget->loadManifest(manifest);
}

} // namespace nx::vms::client::desktop
