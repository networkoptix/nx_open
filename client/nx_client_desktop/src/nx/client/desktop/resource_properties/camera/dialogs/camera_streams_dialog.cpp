#include "camera_streams_dialog.h"
#include "ui_camera_streams_dialog.h"

#include <nx/client/desktop/common/utils/aligner.h>

namespace nx {
namespace client {
namespace desktop {

CameraStreamsDialog::CameraStreamsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraStreamsDialog())
{
    ui->setupUi(this);
    setResizeToContentsMode(Qt::Vertical);

    ui->primaryStreamUrlInputField->setTitle(tr("Primary Stream"));

    ui->secondaryStreamUrlInputField->setTitle(tr("Secondary Stream"));
    ui->secondaryStreamUrlInputField->setPlaceholderText(tr("No secondary stream"));

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->primaryStreamUrlInputField,
        ui->secondaryStreamUrlInputField });
}

CameraStreamsDialog::~CameraStreamsDialog() = default;

CameraStreamsModel CameraStreamsDialog::model() const
{
    return {ui->primaryStreamUrlInputField->text(), ui->secondaryStreamUrlInputField->text()};
}

void CameraStreamsDialog::setModel(const CameraStreamsModel& model)
{
    ui->primaryStreamUrlInputField->setText(model.primaryStreamUrl);
    ui->secondaryStreamUrlInputField->setText(model.secondaryStreamUrl);
}

} // namespace desktop
} // namespace client
} // namespace nx
