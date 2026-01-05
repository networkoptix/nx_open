// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_metadata_settings_widget.h"
#include "ui_export_metadata_settings_widget.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_administration/dialogs/object_type_selection_dialog.h>

namespace nx::vms::client::desktop {

namespace {

const core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light17"}},
    {QIcon::Selected, {.primary = "light15"}},
};

NX_DECLARE_COLORIZED_ICON(kDeleteIcon, "20x20/Outline/delete.svg", kThemeSubstitutions)

} // namespace

ExportMetadataSettingsWidget::ExportMetadataSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMetadataSettingsWidget())
{
    ui->setupUi(this);

    // TODO: @pprivalov enable motion checkbox under VMS-60399
    ui->motionCheckBox->setChecked(false);
    ui->motionCheckBox->setEnabled(false);
    connect(
        ui->objectsCheckBox,
        &QCheckBox::clicked,
        this,
        &ExportMetadataSettingsWidget::emitDataEdited);
    connect(
        ui->displayAttributesCheckBox,
        &QCheckBox::clicked,
        this,
        &ExportMetadataSettingsWidget::emitDataEdited);

    ui->objectTypesButton->setProperty(
        nx::style::Properties::kPushButtonMargin,
        nx::style::Metrics::kStandardPadding);
    connect(
        ui->objectTypesButton,
        &QPushButton::clicked,
        this,
        &ExportMetadataSettingsWidget::openObjectTypeSelectionDialog);

    ui->hasAttributesLabel->hide();
    ui->attributeTextEdit->hide();

    ui->deleteButton->setIcon(qnSkin->icon(kDeleteIcon));

    connect(
        ui->deleteButton,
        &QPushButton::clicked,
        this,
        &ExportMetadataSettingsWidget::deleteClicked);
}

ExportMetadataSettingsWidget::~ExportMetadataSettingsWidget()
{
}

ExportMetadataSettingsWidget::Data ExportMetadataSettingsWidget::buildDataFromUi() const
{
    Data data;
    data.motionChecked = ui->motionCheckBox->isChecked();
    data.objectsChecked = ui->objectsCheckBox->isChecked();
    data.showAttributes = ui->displayAttributesCheckBox->isChecked();
    // Field m_objectTypeSettings is updated from ObjectTypeSelectionDialog.
    data.objectTypeSettings = m_objectTypeSettings;
    return data;
}

void ExportMetadataSettingsWidget::emitDataEdited()
{
    emit dataEdited(buildDataFromUi());
}

void ExportMetadataSettingsWidget::setData(const Data& data)
{
    ui->motionCheckBox->setChecked(data.motionChecked);
    ui->objectsCheckBox->setChecked(data.objectsChecked);

    ui->displayAttributesCheckBox->setVisible(data.objectsChecked);
    ui->objectTypesButton->setVisible(data.objectsChecked);
    ui->objectTypeLabel->setVisible(data.objectsChecked);

    ui->displayAttributesCheckBox->setChecked(data.showAttributes);

    m_objectTypeSettings = data.objectTypeSettings;

    if (!data.objectsChecked)
        return;

    if (data.objectTypeSettings.isAllObjectTypes)
    {
        ui->objectTypesButton->setSelectedAllObjectTypes();
    }
    else
    {
        ui->objectTypesButton->setSelectedObjectTypes(
            appContext()->currentSystemContext(),
            data.objectTypeSettings.objectTypeIds);
    }
}

void ExportMetadataSettingsWidget::openObjectTypeSelectionDialog()
{
    ObjectTypeSelectionDialog::openSelectionDialog(
        m_objectTypeSettings,
        this,
        [&]()
        {
            setData(buildDataFromUi());
            emitDataEdited();
        });
}

} // namespace nx::vms::client::desktop
