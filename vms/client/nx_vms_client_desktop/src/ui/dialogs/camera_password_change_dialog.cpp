#include "camera_password_change_dialog.h"
#include "ui_camera_password_change_dialog.h"

#include <QtWidgets/QPushButton>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/utils/password_analyzer.h>

using namespace nx::vms::client::desktop;

namespace {

void setupPasswordField(InputField& field)
{
    field.setPasswordIndicatorEnabled(true, true, false, nx::utils::cameraPasswordStrength);
    field.setEchoMode(QLineEdit::Password);
    field.reset();
}

TextValidateFunction makeNonCameraUserNameValidator(const QnVirtualCameraResourceList& cameras)
{
    const auto userNames =
        [cameras]()
        {
            QSet<QString> result;
            for (const auto& camera: cameras)
                result.insert(camera->getDefaultAuth().user());
            return result;
        }();

    return
        [userNames](const QString& text)
        {
            static const auto kErrorMessage = QnCameraPasswordChangeDialog::tr(
                "Password should not be equal to camera's user name");
            return userNames.contains(text)
                ? ValidationResult(QValidator::Invalid, kErrorMessage)
                : ValidationResult::kValid;
        };
}

} // namespace

QnCameraPasswordChangeDialog::QnCameraPasswordChangeDialog(
    const QString& password,
    const QnVirtualCameraResourceList& cameras,
    bool forceShowCamerasList,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraPasswordChangeDialog)
{
    ui->setupUi(this);

    if (cameras.isEmpty())
    {
        NX_ASSERT(false, "Cameras list is empty");
        return;
    }

    ui->resourcesWidget->setResources(cameras);
    ui->resourcesPanel->setVisible(forceShowCamerasList || cameras.size() > 1);

    ui->passwordEdit->setTitle(tr("New Password"));
    ui->confirmPasswordEdit->setTitle(tr("Repeat Password"));

    ui->passwordEdit->setValidator(validatorsConcatenator(
        { defaultPasswordValidator(false), makeNonCameraUserNameValidator(cameras) }));
    setupPasswordField(*ui->passwordEdit);

    ui->confirmPasswordEdit->setValidator(defaultConfirmationValidator(
        [this](){ return ui->passwordEdit->text(); }, tr("Passwords do not match.")));
    ui->confirmPasswordEdit->setEchoMode(QLineEdit::Password);

    const auto updateHint =
        [this]()
        {
            static const auto kHintText = tr("Password should be at least 8 symbols long and "
                   "contain different types of characters.");

            const bool showHint = ui->passwordEdit->text().isEmpty()
                && ui->confirmPasswordEdit->text().isEmpty();
            ui->confirmPasswordEdit->setCustomHint(showHint ? kHintText : QString());
        };

    connect(ui->passwordEdit, &InputField::textChanged, this, updateHint);
    connect(ui->confirmPasswordEdit, &InputField::textChanged, this, updateHint);
    ui->passwordEdit->setText(password);
    ui->confirmPasswordEdit->setText(password);
    updateHint();

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->passwordEdit, ui->confirmPasswordEdit });

    setResizeToContentsMode(Qt::Vertical);
}

QnCameraPasswordChangeDialog::~QnCameraPasswordChangeDialog()
{
}

QString QnCameraPasswordChangeDialog::password() const
{
    return ui->passwordEdit->isValid() && ui->confirmPasswordEdit->isValid()
        ? ui->passwordEdit->text()
        : QString();
}

void QnCameraPasswordChangeDialog::accept()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    bool validFields = ui->confirmPasswordEdit->validate();
    validFields &= ui->passwordEdit->validate();
    if (validFields)
        base_type::accept();
}
