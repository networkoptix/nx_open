#include "attach_to_videowall_dialog.h"
#include "ui_attach_to_videowall_dialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QRadioButton>

#include <ui/workaround/qt5_combobox_workaround.h>
#include <client/client_settings.h>

#include <ui/style/warning_style.h>

#include <utils/license_usage_helper.h>

QnAttachToVideowallDialog::QnAttachToVideowallDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnAttachToVideowallDialog),
    m_licenseHelper(new QnVideoWallLicenseUsageHelper())
{
    ui->setupUi(this);

    connect(m_licenseHelper, &QnLicenseUsageHelper::licensesChanged, this, &QnAttachToVideowallDialog::updateLicencesUsage);
    connect(ui->manageWidget, &QnVideowallManageWidget::itemsChanged, this, [this]{
        if (!m_videowall)
            return;
        m_licenseHelper->propose(m_videowall, qnSettings->pcUuid(), ui->manageWidget->proposedItemsCount());
        updateLicencesUsage();
    });
    updateLicencesUsage();
}

QnAttachToVideowallDialog::~QnAttachToVideowallDialog(){}

void QnAttachToVideowallDialog::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    m_videowall = videowall;
    ui->manageWidget->loadFromResource(videowall);
}

void QnAttachToVideowallDialog::submitToResource(const QnVideoWallResourcePtr &videowall) {
    ui->manageWidget->submitToResource(videowall);
}

void QnAttachToVideowallDialog::updateLicencesUsage() {
    QPalette palette = this->palette();
    bool licensesOk = m_licenseHelper->isValid();
    QString licenseUsage = m_licenseHelper->getProposedUsageText(Qn::LC_VideoWall);
    if(!licensesOk) {
        setWarningStyle(&palette);
        licenseUsage += L'\n' + m_licenseHelper->getRequiredLicenseMsg(Qn::LC_VideoWall);
    }
    ui->licensesLabel->setText(licenseUsage);
    ui->licensesLabel->setPalette(palette);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(licensesOk);
}

