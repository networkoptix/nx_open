#include "camera_password_change_dialog.h"
#include "ui_camera_password_change_dialog.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <ui/common/aligner.h>
#include <nx/utils/password_analyzer.h>

namespace {

void setupPasswordField(QnInputField& field)
{
    field.setPasswordIndicatorEnabled(true, true, false, nx::utils::cameraPasswordStrength);
    field.setEchoMode(QLineEdit::Password);
    field.reset();
}

Qn::TextValidateFunction makeNonCameraUserNameValidator(const QnVirtualCameraResourceList& cameras)
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
                "Password shouldn't be equal to camera's user name");
            return userNames.contains(text)
                ? Qn::ValidationResult(QValidator::Invalid, kErrorMessage)
                : Qn::kValidResult;
        };
}

} // namespace

QnCameraPasswordChangeDialog::QnCameraPasswordChangeDialog(
    const QnVirtualCameraResourceList& cameras,
    bool showSingleCameraList,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraPasswordChangeDialog)
{
    ui->setupUi(this);

    if (cameras.isEmpty())
    {
        NX_EXPECT(false, "Cameras list is empty");
        return;
    }

    ui->resourcesWidget->setResources(cameras);
    if (cameras.size() == 1 && !showSingleCameraList)
        ui->resourcesPanel->setVisible(false);

    ui->passwordEdit->setTitle(tr("New Password"));
    ui->confirmPasswordEdit->setTitle(tr("Repeat Password"));

    ui->passwordEdit->setValidator(Qn::validatorsConcatenator(
        { Qn::defaultPasswordValidator(false), makeNonCameraUserNameValidator(cameras) }));
    ui->confirmPasswordEdit->setValidator(Qn::defaultConfirmationValidator(
        [this](){ return ui->passwordEdit->text(); }, tr("Passwords do not match.")));
    setupPasswordField(*ui->passwordEdit);
    setupPasswordField(*ui->confirmPasswordEdit);

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
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
    bool validFields = ui->confirmPasswordEdit->validate();
    validFields &= ui->passwordEdit->validate();
    if (validFields)
        base_type::accept();
}
