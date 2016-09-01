#include "build_number_dialog.h"
#include "ui_build_number_dialog.h"

#include <QtWidgets/QPushButton>

#include <ui/common/aligner.h>
#include <utils/update/update_utils.h>

namespace {

bool checkPassword(int buildNumber, const QString& password)
{
#ifdef _DEBUG
    Q_UNUSED(buildNumber);
    Q_UNUSED(password);
    return false;// true;
#else
    return passwordForBuild((unsigned)buildNumber) == password;
#endif
}

} // namespace

QnBuildNumberDialog::QnBuildNumberDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnBuildNumberDialog)
{
    ui->setupUi(this);
    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);

    ui->buildNumberInputField->setTitle(tr("Build Number"));
    ui->buildNumberInputField->setInputMask(lit("99990"));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setValidator(
        [this](const QString& password)
        {
            int build = buildNumber();
            if (!build)
                return Qn::kValidResult;

            return checkPassword(build, password)
                ? Qn::kValidResult
                : Qn::ValidationResult(tr("The password is incorrect."));
        });

    connect(ui->buildNumberInputField, &QnInputField::textChanged, this, [this, okButton](const QString& text)
    {
        if (!ui->passwordInputField->text().isEmpty())
            ui->passwordInputField->validate();

        okButton->setEnabled(text.toInt() > 0);
    });

    okButton->setEnabled(false);

    auto aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->buildNumberInputField,
        ui->passwordInputField });
}

QnBuildNumberDialog::~QnBuildNumberDialog()
{
}

int QnBuildNumberDialog::buildNumber() const
{
    return ui->buildNumberInputField->text().toInt();
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
