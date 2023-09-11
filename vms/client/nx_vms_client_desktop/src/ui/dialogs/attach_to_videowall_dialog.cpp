// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <boost/range/algorithm/count_if.hpp>

#include <QtWidgets/QPushButton>

#include <core/resource/videowall_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/license/usage_helper.h>
#include <utils/screen_utils.h>

using namespace nx::vms::client::desktop;

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::QnAttachToVideowallDialog),
    m_valid(true)
{
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Videowall_Attach);

    ui->manageWidget->setScreenGeometries(nx::gui::Screens::physicalGeometries());

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
    const auto systemContext = m_videowall->systemContext();

    if (nx::vms::common::saas::saasInitialized(systemContext))
    {
        using namespace nx::vms::common;

        const auto isShutdown = systemContext->saasServiceManager()->saasShutDown();
        if (isShutdown)
        {
            const auto saasState = systemContext->saasServiceManager()->saasState();
            ui->licensesLabel->setText(nx::format(
                tr("%1. To attach to Video Wall, SaaS must be in active state. %2"),
                saas::StringsHelper::shortState(saasState),
                saas::StringsHelper::recommendedAction(saasState)));

            setWarningStyle(ui->licensesLabel);
        }

        ui->licensesLabel->setHidden(!isShutdown);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!isShutdown);
        return;
    }

    /* Calculate how many screens are proposed to be added or removed on local PC. */
    QnUuid pcUuid = appContext()->localSettings()->pcUuid();
    int used = pcUuid.isNull() || m_videowall.isNull()
        ? 0
        : boost::count_if(
            m_videowall->items()->getItems().values(),
            [pcUuid](const QnVideoWallItem& item) { return item.pcUuid == pcUuid; });
    int localScreensChange = ui->manageWidget->proposedItemsCount() - used;

    using namespace nx::vms::license;
    VideoWallLicenseUsageHelper helper(systemContext);
    VideoWallLicenseUsageProposer proposer(systemContext, &helper, localScreensChange);

    QPalette palette = this->palette();
    bool licensesOk = helper.isValid();
    QString licenseUsage = helper.getProposedUsageText(Qn::LC_VideoWall);
    if (!licensesOk)
    {
        setWarningStyle(&palette);
        licenseUsage += '\n' + helper.getRequiredText(Qn::LC_VideoWall);
    }
    ui->licensesLabel->setText(licenseUsage);
    ui->licensesLabel->setPalette(palette);

    /* Allow to save changes if we are removing screens. */
    m_valid = licensesOk || localScreensChange < 0;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_valid);
}
