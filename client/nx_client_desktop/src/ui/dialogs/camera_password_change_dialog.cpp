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
    bool hideSingleCameraList,
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
    if (cameras.size() == 1 && hideSingleCameraList)
        ui->resourcesPanel->setVisible(false);

    ui->passwordEdit->setTitle(tr("New Password"));
    ui->confirmPasswordEdit->setTitle(tr("Repeat Password"));

    setResizeToContentsMode(Qt::Vertical);

    setupPasswordField(*ui->passwordEdit);
    setupPasswordField(*ui->confirmPasswordEdit);
    ui->passwordEdit->setValidator(Qn::validatorsConcatenator(
        { Qn::defaultPasswordValidator(false), makeNonCameraUserNameValidator(cameras) }));
    ui->passwordEdit->reset();
    ui->confirmPasswordEdit->setValidator(Qn::defaultConfirmationValidator(
        [this](){ return ui->passwordEdit->text(); }, tr("Passwords do not match.")));

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({ ui->passwordEdit, ui->confirmPasswordEdit });
}

QnCameraPasswordChangeDialog::~QnCameraPasswordChangeDialog()
{
}

