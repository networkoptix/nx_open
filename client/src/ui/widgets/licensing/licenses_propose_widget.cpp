#include "licenses_propose_widget.h"
#include "ui_licenses_propose_widget.h"

#include <boost/algorithm/cxx11/all_of.hpp>

#include <core/resource/camera_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/common/checkbox_utils.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/license_usage_helper.h>

QnLicensesProposeWidget::QnLicensesProposeWidget(QWidget *parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::LicensesProposeWidget)
{
    ui->setupUi(this);

    QnCheckbox::autoCleanTristate(ui->analogViewCheckBox);

    connect(ui->analogViewCheckBox,     &QCheckBox::stateChanged,              this,   &QnLicensesProposeWidget::updateLicenseText);
    connect(ui->analogViewCheckBox,     &QCheckBox::stateChanged,              this,   &QnLicensesProposeWidget::changed);

    auto updateLicensesIfNeeded = [this] { 
        if (!isVisible())
            return;
        updateLicenseText();
    };

    QnCamLicenseUsageHelper helper;
    ui->licensesUsageWidget->init(&helper);

    QnCamLicenseUsageWatcher* camerasUsageWatcher = new QnCamLicenseUsageWatcher(this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,  updateLicensesIfNeeded);
}

QnLicensesProposeWidget::~QnLicensesProposeWidget()
{}

void QnLicensesProposeWidget::afterContextInitialized() {
    connect(context(), &QnWorkbenchContext::userChanged,        this,   &QnLicensesProposeWidget::updateLicensesButtonVisible);
    connect(ui->moreLicensesButton,     &QPushButton::clicked,  this,   [this]{menu()->trigger(Qn::PreferencesLicensesTabAction);});
    updateLicensesButtonVisible();
}


void QnLicensesProposeWidget::updateLicenseText() {
    QnCamLicenseUsageHelper helper;

    switch(ui->analogViewCheckBox->checkState()) {
    case Qt::Checked:
        helper.propose(m_cameras, true);
        break;
    case Qt::Unchecked:
        helper.propose(m_cameras, false);
        break;
    default:
        break;
    }

    ui->licensesUsageWidget->loadData(&helper);
}

void QnLicensesProposeWidget::updateLicensesButtonVisible() {
    ui->moreLicensesButton->setVisible(context()->accessController()->globalPermissions() & Qn::GlobalProtectedPermission);
}

QnVirtualCameraResourceList QnLicensesProposeWidget::cameras() const {
    return m_cameras;
}

void QnLicensesProposeWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    m_cameras = cameras;

    bool licenseUsed = cameras.isEmpty() ? false : cameras.front()->isLicenseUsed();

    bool sameValue = boost::algorithm::all_of(cameras, [licenseUsed](const QnVirtualCameraResourcePtr &camera) { 
        return camera->isLicenseUsed() == licenseUsed; 
    });
    QnCheckbox::setupTristateCheckbox(ui->analogViewCheckBox, sameValue, licenseUsed);

    updateLicenseText();
}

Qt::CheckState QnLicensesProposeWidget::state() const {
    return ui->analogViewCheckBox->checkState();
}

void QnLicensesProposeWidget::setState(Qt::CheckState value) {
    if (value == Qt::PartiallyChecked) {
        ui->analogViewCheckBox->setTristate(true);
        ui->analogViewCheckBox->setCheckState(Qt::PartiallyChecked);
    } else {
        ui->analogViewCheckBox->setTristate(false);
        ui->analogViewCheckBox->setChecked(value == Qt::Checked);
    }
    updateLicenseText();
}
