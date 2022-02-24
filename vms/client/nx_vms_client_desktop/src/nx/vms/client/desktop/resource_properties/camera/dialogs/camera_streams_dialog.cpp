// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_streams_dialog.h"
#include "ui_camera_streams_dialog.h"

#include <QtCore/QUrl>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/widgets/line_edit_field.h>

namespace nx::vms::client::desktop {

CameraStreamsDialog::CameraStreamsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraStreamsDialog())
{
    ui->setupUi(this);
    setResizeToContentsMode(Qt::Vertical);

    ui->primaryStreamUrlInputField->setTitle(tr("Primary Stream"));
    ui->primaryStreamUrlInputField->setValidator([this](const QString&) -> ValidationResult
    {
        ValidationResult result(QValidator::Acceptable);

        if (!QUrl(ui->primaryStreamUrlInputField->text()).isValid())
        {
            result.state = QValidator::Invalid;
            result.errorMessage = tr("Invalid stream address");
        }

        return result;
    });

    ui->secondaryStreamUrlInputField->setTitle(tr("Secondary Stream"));
    ui->secondaryStreamUrlInputField->setPlaceholderText(tr("No secondary stream"));
    ui->secondaryStreamUrlInputField->setValidator([this](const QString&) -> ValidationResult
    {
        ValidationResult result(QValidator::Acceptable);

        const QUrl secondaryStreamUrl(ui->secondaryStreamUrlInputField->text());

        if (secondaryStreamUrl.isEmpty())
            ;
        else if (!secondaryStreamUrl.isValid())
        {
            result.state = QValidator::Invalid;
            result.errorMessage = tr("Invalid stream address");
        }
        else if (const QUrl primaryStreamUrl(ui->primaryStreamUrlInputField->text());
            primaryStreamUrl.scheme() != secondaryStreamUrl.scheme())
        {
            result.state = QValidator::Invalid;
            result.errorMessage = tr("Streaming protocol mismatch");
        }

        return result;
    });

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<LineEditField>(LineEditField::createLabelWidthAccessor());
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
