// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_intro_dialog.h"
#include "ui_cloud_layouts_intro_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/style/skin.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/style/custom_style.h>

namespace nx::vms::client::desktop {

CloudLayoutsIntroDialog::CloudLayoutsIntroDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::CloudLayoutsIntroDialog())
{
    ui->setupUi(this);
    ui->imageLabel->setPixmap(qnSkin->pixmap("promo/cloud_layouts_promo.png"));
    ui->captionLabel->setText(tr(
        "Introducing %1 Layouts",
        "%1 is the short cloud name (like Cloud)").arg(nx::branding::shortCloudName()));
    ui->descriptionLabel->setText(tr(
        "Adding a Camera from another System transforms the Layout to a %1 Layout",
        "%1 is the short cloud name (like Cloud)")
            .arg(nx::branding::shortCloudName()));

    auto acceptButton = ui->buttonBox->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    setAccentStyle(acceptButton);
}

CloudLayoutsIntroDialog::~CloudLayoutsIntroDialog()
{
}

bool CloudLayoutsIntroDialog::doNotShowAgainChecked() const
{
    return ui->doNotShowAgainCheckBox->isChecked();
}

} // namespace nx::vms::client::desktop
