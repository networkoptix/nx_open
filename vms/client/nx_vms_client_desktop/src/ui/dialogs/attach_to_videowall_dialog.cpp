#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <QtWidgets/QPushButton>

#include <boost/range/algorithm/count_if.hpp>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/custom_style.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/license_usage_helper.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>

#include <utils/screen_utils.h>

using namespace nx::vms::client::desktop;

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnAttachToVideowallDialog),
    m_valid(true)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Videowall_Attach_Help);

    ui->manageWidget->setScreenGeometries(nx::gui::Screens::geometries());

    connect(
        ui->manageWidget,
        &QnVideowallManageWidget::itemsChanged,
        this,
        [this]
        {
            if (!m_videowall)
                return;
            updateLicencesUsage();
        });
}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog()
{
}

void QnAttachToVideowallDialog::loadFromResource(const QnVideoWallResourcePtr& videowall)
{
    m_videowall = videowall;
    ui->manageWidget->loadFromResource(videowall);
    updateLicencesUsage();
}

void QnAttachToVideowallDialog::submitToResource(const QnVideoWallResourcePtr& videowall)
{
    updateLicencesUsage();
    if (m_valid)
        ui->manageWidget->submitToResource(videowall);
}

void QnAttachToVideowallDialog::updateLicencesUsage()
{
    /* Calculate how many screens are proposed to be added or removed on local PC. */
    QnUuid pcUuid = qnSettings->pcUuid();
    int used = pcUuid.isNull() || m_videowall.isNull()
        ? 0
        : boost::count_if(
            m_videowall->items()->getItems(),
            [pcUuid](const QnVideoWallItem& item) { return item.pcUuid == pcUuid; });
    int localScreensChange = ui->manageWidget->proposedItemsCount() - used;

    license::VideoWallLicenseValidator validator(commonModule());
    QnVideoWallLicenseUsageHelper helper(commonModule());
    helper.setCustomValidator(&validator);
    QnVideoWallLicenseUsageProposer proposer(&helper, localScreensChange, 0);

    QPalette palette = this->palette();
    bool licensesOk = helper.isValid();
    QString licenseUsage = helper.getProposedUsageText(Qn::LC_VideoWall);
    if (!licensesOk)
    {
        setWarningStyle(&palette);
        licenseUsage += L'\n' + helper.getRequiredText(Qn::LC_VideoWall);
    }
    ui->licensesLabel->setText(licenseUsage);
    ui->licensesLabel->setPalette(palette);

    /* Allow to save changes if we are removing screens. */
    m_valid = licensesOk || localScreensChange < 0;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_valid);
}
