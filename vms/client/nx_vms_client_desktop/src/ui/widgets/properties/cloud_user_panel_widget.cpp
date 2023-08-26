// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_user_panel_widget.h"
#include "ui_cloud_user_panel_widget.h"

#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/common/html/html.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

const int kCloudUserFontSizePixels = 18;
const auto kCloudUserFontWeight = QFont::Light;

} // namespace


QnCloudUserPanelWidget::QnCloudUserPanelWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::CloudUserPanelWidget())
{
    ui->setupUi(this);

    ui->iconLabel->setPixmap(qnSkin->pixmap("cloud/cloud_32_thin_selected.png"));

    QFont font;
    font.setPixelSize(kCloudUserFontSizePixels);
    font.setWeight(kCloudUserFontWeight);
    ui->emailLabel->setFont(font);
    ui->emailLabel->setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    ui->emailLabel->setForegroundRole(QPalette::Text);

    static const QMargins kSpacingByContentsMargin(
        0, nx::style::Metrics::kDefaultLayoutSpacing.height(), 0, 0);
    ui->manageAccountLabel->setContentsMargins(ui->manageAccountLabel->contentsMargins()
        + kSpacingByContentsMargin);

    updateManageAccountLink();
    connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::statusChanged, this,
        &QnCloudUserPanelWidget::updateManageAccountLink);
}

QnCloudUserPanelWidget::~QnCloudUserPanelWidget()
{
}

bool QnCloudUserPanelWidget::isManageLinkShown() const
{
    return !ui->manageAccountLabel->isHidden();
}

void QnCloudUserPanelWidget::setManageLinkShown(bool value)
{
    ui->manageAccountLabel->setHidden(!value);
}

QString QnCloudUserPanelWidget::email() const
{
    return ui->emailLabel->text();
}

void QnCloudUserPanelWidget::setEmail(const QString& value)
{
    ui->emailLabel->setText(value);
}

QString QnCloudUserPanelWidget::fullName() const
{
    return ui->nameLabel->text();
}

void QnCloudUserPanelWidget::setFullName(const QString& value)
{
    ui->nameLabel->setText(value);
    ui->nameLabel->setVisible(!value.trimmed().isEmpty());
}

AbstractAccessor* QnCloudUserPanelWidget::createIconWidthAccessor()
{
    const auto getLabelWidth =
        [](const QObject* object) -> QVariant
        {
            if (auto panel = qobject_cast<const QnCloudUserPanelWidget*>(object))
                return panel->ui->iconLabel->minimumSizeHint().width();

            return QVariant();
        };

    const auto setLabelWidth =
        [](QObject* object, const QVariant& value)
        {
            if (auto panel = qobject_cast<const QnCloudUserPanelWidget*>(object))
                panel->ui->iconLabel->setFixedWidth(value.toInt());
        };

    return newAccessor(getLabelWidth, setLabelWidth);
}

void QnCloudUserPanelWidget::updateManageAccountLink()
{
    if (qnCloudStatusWatcher->status() == core::CloudStatusWatcher::Status::LoggedOut)
    {
        ui->manageAccountLabel->setText(tr("Account Settings"));
        return;
    }

    using nx::vms::utils::SystemUri;
    core::CloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);
    ui->manageAccountLabel->setText(nx::vms::common::html::link(
        tr("Account Settings"),
        urlHelper.accountManagementUrl()));
}
