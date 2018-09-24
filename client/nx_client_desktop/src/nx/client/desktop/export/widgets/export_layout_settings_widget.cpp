#include "export_layout_settings_widget.h"
#include "ui_export_layout_settings_widget.h"

#include <nx/client/desktop/common/widgets/password_preview_button.h>

#include <QtWidgets/QStyle>

namespace nx {
namespace client {
namespace desktop {

ExportLayoutSettingsWidget::ExportLayoutSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportLayoutSettingsWidget())
{
    ui->setupUi(this);

    ui->passwordEdit->setValidator(defaultPasswordValidator(false, tr("Please enter the password.")));

    ui->passwordSpacer->changeSize(
        style()->pixelMetric(QStyle::PM_IndicatorWidth)
            + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing),
        10, QSizePolicy::Fixed);

    PasswordPreviewButton::createInline(ui->passwordEdit->lineEdit());

    auto encryptionStatusChanged = [this](bool on) { ui->passwordEdit->setVisible(on); };
    connect(ui->cryptCheckBox, &QCheckBox::toggled,
        this, encryptionStatusChanged);
    encryptionStatusChanged(ui->cryptCheckBox->isChecked());

    connect(ui->readOnlyCheckBox, &QCheckBox::clicked,
        this, &ExportLayoutSettingsWidget::emitDataChanged);
    connect(ui->cryptCheckBox, &QCheckBox::clicked,
        this, &ExportLayoutSettingsWidget::emitDataChanged);
    connect(ui->passwordEdit->lineEdit(), &QLineEdit::textEdited,
        this, [this](const QString&) { emitDataChanged(); });
}

ExportLayoutSettingsWidget::~ExportLayoutSettingsWidget()
{
}

void ExportLayoutSettingsWidget::emitDataChanged()
{
    Data data;
    data.readOnly = ui->readOnlyCheckBox->isChecked();
    data.cryptVideo = ui->cryptCheckBox->isChecked();
    data.password = ui->passwordEdit->text().trimmed(); //< Trimming the password!
    emit dataChanged(data);
}

void ExportLayoutSettingsWidget::setData(const Data& data)
{
    ui->readOnlyCheckBox->setChecked(data.readOnly);
    ui->cryptCheckBox->setChecked(data.cryptVideo);
    ui->passwordEdit->setText(data.password);
}

bool ExportLayoutSettingsWidget::validate()
{
    return ui->passwordEdit->validate();
}

} // namespace desktop
} // namespace client
} // namespace nx
