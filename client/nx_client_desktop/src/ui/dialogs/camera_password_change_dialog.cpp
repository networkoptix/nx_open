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

} // namespace

QnCameraPasswordChangeDialog::QnCameraPasswordChangeDialog(
    const QnVirtualCameraResourceList& cameras,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraPasswordChangeDialog)
{
    ui->setupUi(this);
    if (cameras.size() > 1)
        ui->resourcesWidget->setResources(cameras);
    else
        ui->resourcesPanel->setVisible(false);

    ui->passwordEdit->setTitle(tr("New Password"));
    ui->confirmPasswordEdit->setTitle(tr("Repeat Password"));

    setResizeToContentsMode(Qt::Vertical);

    setupPasswordField(*ui->passwordEdit);
    setupPasswordField(*ui->confirmPasswordEdit);
    ui->passwordEdit->setValidator(Qn::defaultPasswordValidator(false));
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

