// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_login_placeholder.h"
#include "ui_oauth_login_placeholder.h"

#include <nx/branding.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <ui/common/palette.h>

namespace {

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kUnavalableTheme = {
    {QnIcon::Normal, {.primary = "light16", .secondary = "red_l"}}};

NX_DECLARE_COLORIZED_ICON(
    kCloudUnavailableIcon, "128x128/Outline/cloud_unavailable.svg", kUnavalableTheme)
} // namespace

namespace nx::vms::client::desktop {

OauthLoginPlaceholder::OauthLoginPlaceholder(QWidget* parent):
    base_type(parent),
    ui(std::make_unique<Ui::OauthLoginPlaceholder>())
{
    ui->setupUi(this);

    ui->errorImageLabel->setPixmap(qnSkin->icon(kCloudUnavailableIcon).pixmap(128, 128));

    auto errorFont = ui->errorTextLabel->font();
    errorFont.setPixelSize(24);
    ui->errorTextLabel->setFont(errorFont);
    setPaletteColor(ui->errorTextLabel, QPalette::WindowText, core::colorTheme()->color("light1"));
    ui->errorTextLabel->setText(tr("Cannot connect to %1", "%1 is the cloud name like Nx Cloud")
        .arg(nx::branding::cloudName()));

    layout()->setAlignment(ui->retryButton, Qt::AlignCenter);
    setAccentStyle(ui->retryButton);
    connect(ui->retryButton, &QPushButton::clicked, this, &OauthLoginPlaceholder::retryRequested);
}

OauthLoginPlaceholder::~OauthLoginPlaceholder()
{
}

} // namespace nx::vms::client::desktop
