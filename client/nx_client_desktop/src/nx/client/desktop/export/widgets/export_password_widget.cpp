#include "export_password_widget.h"
#include "ui_export_password_widget.h"

#include <QtWidgets/QStyle>

namespace nx {
namespace client {
namespace desktop {

ExportPasswordWidget::ExportPasswordWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::ExportPasswordWidget)
{
    ui->setupUi(this);

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
        this, &ExportPasswordWidget::emitDataChanged);
    connect(ui->passwordEdit->lineEdit(), &QLineEdit::textEdited,
        this, [this](const QString&) { emitDataChanged(); });
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

void ExportPasswordWidget::emitDataChanged()
{
    Data data;
    data.cryptVideo = ui->cryptCheckBox->isChecked();
    data.password = ui->passwordEdit->text().trimmed(); //< Trimming the password!
    emit dataChanged(data);
}

} // namespace desktop
} // namespace client
} // namespace nx
