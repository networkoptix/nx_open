#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QRadioButton>

#include <boost/range/algorithm/count_if.hpp>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/warning_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/license_usage_helper.h>

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnAttachToVideowallDialog),
    m_valid(true)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Videowall_Attach_Help);
    connect(ui->manageWidget, &QnVideowallManageWidget::itemsChanged, this, [this]{
        if (!m_videowall)
            return;
        updateLicencesUsage();
    });
}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog(){}

void QnAttachToVideowallDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    m_videowall = videowall;
    ui->manageWidget->loadFromResource(videowall);
    updateLicencesUsage();
}

void QnAttachToVideowallDialog::submitToResource(const QnVideoWallResourcePtr &videowall) {
    updateLicencesUsage();
    if (m_valid)
        ui->manageWidget->submitToResource(videowall);
}

void QnAttachToVideowallDialog::updateLicencesUsage() {
    /* Calculate how many screens are proposed to be added or removed on local PC. */
    QnUuid pcUuid = qnSettings->pcUuid();
    int used = pcUuid.isNull() || m_videowall.isNull()
        ? 0
        : boost::count_if(m_videowall->items()->getItems(), [pcUuid](const QnVideoWallItem &item){return item.pcUuid == pcUuid;});
    int localScreensChange = ui->manageWidget->proposedItemsCount() - used;

    QnVideoWallLicenseUsageHelper helper;
    QnVideoWallLicenseUsageProposer proposer(&helper, localScreensChange, 0);

    QPalette palette = this->palette();
    bool licensesOk = helper.isValid();
    QString licenseUsage = helper.getProposedUsageText(Qn::LC_VideoWall);
    if(!licensesOk) {
        setWarningStyle(&palette);
        licenseUsage += L'\n' + helper.getRequiredText(Qn::LC_VideoWall);
    }
    ui->licensesLabel->setText(licenseUsage);
    ui->licensesLabel->setPalette(palette);

    /* Allow to save changes if we are removing screens. */
    m_valid = licensesOk || localScreensChange < 0;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_valid);
}

