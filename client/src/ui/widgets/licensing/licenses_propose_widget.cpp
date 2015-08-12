#include "licenses_propose_widget.h"
#include "ui_licenses_propose_widget.h"

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/count_if.hpp>

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

    QnCheckbox::autoCleanTristate(ui->useLicenseCheckBox);

    connect(ui->useLicenseCheckBox,     &QCheckBox::stateChanged,              this,   &QnLicensesProposeWidget::updateLicenseText);
    connect(ui->useLicenseCheckBox,     &QCheckBox::stateChanged,              this,   &QnLicensesProposeWidget::changed);

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

    switch(ui->useLicenseCheckBox->checkState()) {
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

    int analogCameras = boost::count_if(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {
        return camera->isDtsBased(); }
    );
    int ioModules = boost::count_if(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {
        return camera->hasCameraCapabilities(Qn::IOModuleCapability);
    });

    setVisible(analogCameras > 0 || ioModules > 0);

    QString title;
    if (analogCameras == m_cameras.size())
        title = tr("Use analog licenses to view these %n cameras", "", analogCameras);
    else if (ioModules == m_cameras.size())
        title = tr("Use I/O licenses to enable these %n modules", "", ioModules);
    else if (ioModules > 0)
        title = tr("Use licenses for selected cameras and modules");
    else
        title = tr("Use licenses for selected cameras");
    ui->useLicenseCheckBox->setText(title);

    bool licenseUsed = m_cameras.isEmpty() ? false : m_cameras.front()->isLicenseUsed();

    bool sameValue = boost::algorithm::all_of(m_cameras, [licenseUsed](const QnVirtualCameraResourcePtr &camera) { 
        return camera->isLicenseUsed() == licenseUsed; 
    });
    QnCheckbox::setupTristateCheckbox(ui->useLicenseCheckBox, sameValue, licenseUsed);

    updateLicenseText();
}

Qt::CheckState QnLicensesProposeWidget::state() const {
    return ui->useLicenseCheckBox->checkState();
}

void QnLicensesProposeWidget::setState(Qt::CheckState value) {
    if (value == Qt::PartiallyChecked) {
        ui->useLicenseCheckBox->setTristate(true);
        ui->useLicenseCheckBox->setCheckState(Qt::PartiallyChecked);
    } else {
        ui->useLicenseCheckBox->setTristate(false);
        ui->useLicenseCheckBox->setChecked(value == Qt::Checked);
    }
    updateLicenseText();
}
