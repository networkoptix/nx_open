#include "build_number_dialog.h"
#include "ui_build_number_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <utils/update/update_utils.h>

using namespace nx::vms::client::desktop;

namespace {

bool checkPassword(const QString& build, const QString& password)
{
#ifdef _DEBUG
    Q_UNUSED(build);
    Q_UNUSED(password);
    qDebug() << passwordForBuild(build);
    return true;
#else
    return passwordForBuild(build) == password;
#endif
}

} // namespace

QnBuildNumberDialog::QnBuildNumberDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnBuildNumberDialog)
{
    ui->setupUi(this);
    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Select Build"));

    ui->buildNumberInputField->setTitle(tr("Build Number"));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setValidator(
        [this](const QString& password)
        {
            const auto buildOrChangeset = changeset();
            if (buildOrChangeset.isEmpty())
                return ValidationResult::kValid;

            return checkPassword(buildOrChangeset.trimmed(), password.trimmed())
                ? ValidationResult::kValid
                : ValidationResult(tr("The password is incorrect."));
        });

    connect(ui->buildNumberInputField, &InputField::textChanged, this,
        [this, okButton](const QString& text)
        {
            if (!ui->passwordInputField->text().isEmpty())
                ui->passwordInputField->validate();

            okButton->setEnabled(!text.isEmpty());
        });

    okButton->setEnabled(false);

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->buildNumberInputField,
        ui->passwordInputField });

    setResizeToContentsMode(Qt::Vertical);
}

QnBuildNumberDialog::~QnBuildNumberDialog()
{
}

int QnBuildNumberDialog::buildNumber() const
{
    return ui->buildNumberInputField->text().toInt();
}

QString QnBuildNumberDialog::changeset() const
{
    return ui->buildNumberInputField->text().trimmed();
}

QString QnBuildNumberDialog::password() const
{
    return ui->passwordInputField->text();
}

void QnBuildNumberDialog::accept()
{
    if (ui->passwordInputField->validate())
        base_type::accept();
}
