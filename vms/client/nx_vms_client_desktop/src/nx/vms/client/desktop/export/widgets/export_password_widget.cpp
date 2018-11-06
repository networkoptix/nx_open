#include "export_password_widget.h"
#include "ui_export_password_widget.h"

#include <QtWidgets/QStyle>

namespace nx::vms::client::desktop {

ExportPasswordWidget::ExportPasswordWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::ExportPasswordWidget)
{
    ui->setupUi(this);

    ui->passwordEdit->useForPassword();
    ui->passwordEdit->setValidator(defaultPasswordValidator(false, tr("Please enter the password.")));

    ui->passwordSpacer->changeSize(
        style()->pixelMetric(QStyle::PM_IndicatorWidth)
            + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing),
        10, QSizePolicy::Fixed);

    auto encryptionStatusChanged = [this](bool on) { ui->passwordEdit->setVisible(on); };
    connect(ui->cryptCheckBox, &QCheckBox::toggled,
        this, encryptionStatusChanged);
    encryptionStatusChanged(ui->cryptCheckBox->isChecked());

    connect(ui->cryptCheckBox, &QCheckBox::clicked,
        this, &ExportPasswordWidget::emitDataEdited);
    connect(ui->passwordEdit->lineEdit(), &QLineEdit::textEdited,
        this, [this](const QString&) { emitDataEdited(); });
}

ExportPasswordWidget::~ExportPasswordWidget()
{
    delete ui;
}

void ExportPasswordWidget::setData(const Data& data)
{
    ui->cryptCheckBox->setChecked(data.cryptVideo);
    ui->passwordEdit->setText(data.password);
}

bool ExportPasswordWidget::validate()
{
    return !ui->cryptCheckBox->isChecked() || ui->passwordEdit->validate(); // Order matters.
}

void ExportPasswordWidget::emitDataEdited()
{
    Data data;
    data.cryptVideo = ui->cryptCheckBox->isChecked();
    data.password = ui->passwordEdit->text().trimmed(); //< Trimming the password!
    emit dataEdited(data);
}

} // namespace nx::vms::client::desktop
