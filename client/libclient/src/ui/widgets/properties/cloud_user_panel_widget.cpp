#include "cloud_user_panel_widget.h"
#include "ui_cloud_user_panel_widget.h"

#include <helpers/cloud_url_helper.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>

#include <utils/common/html.h>

namespace {

const int kCloudUserFontSizePixels = 18;
const int kCloudUserFontWeight = QFont::Light;

} // namespace


QnCloudUserPanelWidget::QnCloudUserPanelWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::CloudUserPanelWidget())
{
    ui->setupUi(this);

    ui->iconLabel->setPixmap(qnSkin->pixmap("user_settings/cloud_user_icon.png"));

    QFont font;
    font.setPixelSize(kCloudUserFontSizePixels);
    font.setWeight(kCloudUserFontWeight);
    ui->emailLabel->setFont(font);
    ui->emailLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->emailLabel->setForegroundRole(QPalette::Text);

    using nx::vms::utils::SystemUri;
    QnCloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);
    ui->manageAccountLabel->setText(makeHref(tr("Account Settings"),
        urlHelper.accountManagementUrl()));
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
