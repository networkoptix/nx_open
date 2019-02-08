#include "licenses_propose_widget.h"
#include "ui_licenses_propose_widget.h"

#include <boost/range/algorithm/count_if.hpp>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/license_usage_helper.h>

using namespace nx::vms::client::desktop;

QnLicensesProposeWidget::QnLicensesProposeWidget(QWidget *parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    ui(new Ui::LicensesProposeWidget)
{
    ui->setupUi(this);

    check_box_utils::autoClearTristate(ui->useLicenseCheckBox);

    connect(ui->useLicenseCheckBox, &QCheckBox::stateChanged, this,
        [this]
        {
            updateLicenseText();
            if (isUpdating())
                return;
            emit changed();
        });

    auto updateLicensesIfNeeded =
        [this]
        {
            if (!isVisible())
                return;
            updateLicenseText();
        };

    QnCamLicenseUsageHelper helper(commonModule());
    ui->licensesUsageWidget->init(&helper);

    auto camerasUsageWatcher = new QnCamLicenseUsageWatcher(commonModule(), this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        updateLicensesIfNeeded);
}

QnLicensesProposeWidget::~QnLicensesProposeWidget()
{}

void QnLicensesProposeWidget::afterContextInitialized()
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnLicensesProposeWidget::updateLicensesButtonVisible);
    connect(ui->moreLicensesButton, &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::PreferencesLicensesTabAction); });
    updateLicensesButtonVisible();
}

void QnLicensesProposeWidget::updateLicenseText()
{
    if (isUpdating())
        return;

    QnCamLicenseUsageHelper helper(commonModule());

    switch (ui->useLicenseCheckBox->checkState())
    {
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

void QnLicensesProposeWidget::updateLicensesButtonVisible()
{
    ui->moreLicensesButton->setVisible(
        accessController()->hasGlobalPermission(GlobalPermission::admin));
}

QnVirtualCameraResourceList QnLicensesProposeWidget::cameras() const
{
    return m_cameras;
}

void QnLicensesProposeWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_cameras = cameras;
}

void QnLicensesProposeWidget::updateFromResources()
{
    QnUpdatableGuard<QnLicensesProposeWidget> guard(this);

    int analogCameras = boost::count_if(m_cameras,
        [](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->isDtsBased();
        });

    int ioModules = boost::count_if(m_cameras,
        [](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->isIOModule();
        });

    setVisible(analogCameras > 0 || ioModules > 0);

    QString title;
    if (analogCameras == m_cameras.size())
    {
        title = tr("Use licenses to view these %n cameras", "", analogCameras);
    }
    else
    {
        title = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Use licenses for selected %n devices", "", m_cameras.size()),
                tr("Use licenses for selected %n cameras", "", m_cameras.size()),
                tr("Use licenses for selected %n I/O modules", "", m_cameras.size())
            ), m_cameras
        );
    }
    ui->useLicenseCheckBox->setText(title);

    bool licenseUsed = m_cameras.isEmpty() ? false : m_cameras.front()->isLicenseUsed();

    bool sameValue = boost::algorithm::all_of(m_cameras,
        [licenseUsed](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->isLicenseUsed() == licenseUsed;
        });
    check_box_utils::setupTristateCheckbox(ui->useLicenseCheckBox, sameValue, licenseUsed);
}

Qt::CheckState QnLicensesProposeWidget::state() const
{
    return ui->useLicenseCheckBox->checkState();
}

void QnLicensesProposeWidget::setState(Qt::CheckState value)
{
    if (value == Qt::PartiallyChecked)
    {
        ui->useLicenseCheckBox->setTristate(true);
        ui->useLicenseCheckBox->setCheckState(Qt::PartiallyChecked);
    }
    else
    {
        ui->useLicenseCheckBox->setTristate(false);
        ui->useLicenseCheckBox->setChecked(value == Qt::Checked);
    }
    updateLicenseText();
}

void QnLicensesProposeWidget::afterUpdate()
{
    updateLicenseText();
}
