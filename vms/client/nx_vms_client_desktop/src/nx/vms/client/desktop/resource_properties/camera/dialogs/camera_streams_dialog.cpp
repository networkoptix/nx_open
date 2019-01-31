#include "camera_streams_dialog.h"
#include "ui_camera_streams_dialog.h"

#include <nx/vms/client/desktop/common/utils/aligner.h>

namespace nx::vms::client::desktop {

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
    aligner->addWidgets({ui->primaryStreamUrlInputField, ui->secondaryStreamUrlInputField});
}

CameraStreamsDialog::~CameraStreamsDialog()
{
}

CameraStreamUrls CameraStreamsDialog::streams() const
{
    return CameraStreamUrls{ui->primaryStreamUrlInputField->text().trimmed(),
        ui->secondaryStreamUrlInputField->text().trimmed()};
}

void CameraStreamsDialog::setStreams(const CameraStreamUrls& value)
{
    ui->primaryStreamUrlInputField->setText(value.primaryStreamUrl);
    ui->secondaryStreamUrlInputField->setText(value.secondaryStreamUrl);
}

} // namespace nx::vms::client::desktop
