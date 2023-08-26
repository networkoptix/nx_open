// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_intro_dialog.h"
#include "ui_cloud_layouts_intro_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/branding.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

CloudLayoutsIntroDialog::CloudLayoutsIntroDialog(
    Mode mode,
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    ui(new Ui::CloudLayoutsIntroDialog())
{
    ui->setupUi(this);
    ui->imageLabel->setPixmap(qnSkin->pixmap("promo/cloud_layouts_intro.png"));
    ui->availableActionsIconLabel->setPixmap(qnSkin->pixmap("promo/available.svg"));
    ui->inFutureIconLabel->setPixmap(qnSkin->pixmap("promo/future.svg"));
    ui->captionLabel->setText(tr(
        "Introducing %1 Layouts",
        "%1 is the short cloud name (like Cloud)").arg(nx::branding::shortCloudName()));
    ui->descriptionLabel->setText(tr(
        "%1 Layouts are <b>cross-system layouts</b> that allow you to work with devices from "
        "different Systems. Currently, only some features of regular layouts are available, but "
        "we will continue to expand the capabilities of %1 Layouts in future versions")
        .arg(nx::branding::shortCloudName()));
    ui->helpIconLabel->setPixmap(
        qnSkin->icon("buttons/context_hint_16.svg", HintButton::kIconSubstitutions)
            .pixmap(QSize(16, 16)));
    ui->helpLabel->setText(
        tr("Read more on the %1").arg(common::html::localLink(tr("help page"))));
    connect(ui->helpLabel, &QLabel::linkActivated,
        [](){ HelpHandler::openHelpTopic(HelpTopic::Id::CloudLayoutsIntroduction); });
    setHelpTopic(this, HelpTopic::Id::CloudLayoutsIntroductionAssign);

    QColor captionTextColor = core::colorTheme()->color("light1");
    setPaletteColor(ui->captionLabel, QPalette::WindowText, captionTextColor);
    setPaletteColor(ui->availableActionsLabel, QPalette::WindowText, captionTextColor);
    setPaletteColor(ui->inFutureLabel, QPalette::WindowText, captionTextColor);

    if (mode == Mode::confirmation)
    {
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        setAccentStyle(this, QDialogButtonBox::Ok);
    }
    else
    {
        ui->doNotShowAgainCheckBox->setVisible(false);
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok);
    }
}

CloudLayoutsIntroDialog::~CloudLayoutsIntroDialog()
{
}

bool CloudLayoutsIntroDialog::doNotShowAgainChecked() const
{
    return ui->doNotShowAgainCheckBox->isChecked();
}

bool CloudLayoutsIntroDialog::confirm()
{
    if (showOnceSettings()->cloudLayoutsPromo())
        return true;

    CloudLayoutsIntroDialog introDialog(
        Mode::confirmation,
        appContext()->mainWindowContext()->mainWindowWidget());
    const bool result = (introDialog.exec() == QDialog::Accepted);

    if (result && introDialog.doNotShowAgainChecked())
        showOnceSettings()->cloudLayoutsPromo = true;

    return result;
}

} // namespace nx::vms::client::desktop
