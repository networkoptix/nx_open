#include "fullscreen_camera_action_widget.h"
#include "ui_fullscreen_camera_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <business/business_resource_validation.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <ui/dialogs/resource_selection_dialog.h>

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/client/desktop/event_rules/helpers/fullscreen_action_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>

using namespace nx::vms::client::desktop;

QnFullscreenCameraActionWidget::QnFullscreenCameraActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FullscreenCameraActionWidget)
{
    ui->setupUi(this);
    ui->layoutWarningWidget->setVisible(false);
    setWarningStyle(ui->layoutWarningWidget);

    connect(
        ui->useSourceCheckBox,
        &QCheckBox::clicked,
        this,
        &QnFullscreenCameraActionWidget::paramsChanged);

    connect(ui->selectCameraButton, &QPushButton::clicked,
        this, &QnFullscreenCameraActionWidget::openCameraSelectionDialog);

    connect(ui->selectLayoutButton, &QPushButton::clicked,
        this, &QnFullscreenCameraActionWidget::openLayoutSelectionDialog);
}

QnFullscreenCameraActionWidget::~QnFullscreenCameraActionWidget()
{
}

void QnFullscreenCameraActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectCameraButton);
    setTabOrder(ui->selectCameraButton, ui->useSourceCheckBox);
    setTabOrder(ui->useSourceCheckBox, ui->selectLayoutButton);
    setTabOrder(ui->selectLayoutButton, after);
}

void QnFullscreenCameraActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventType))
    {
        ui->useSourceCheckBox->setEnabled(model()->canUseSourceCamera());
    }

    if (fields.testFlag(Field::actionParams))
    {
        ui->useSourceCheckBox->setChecked(model()->isUsingSourceCamera());
    }

    if (fields & (Field::eventType | Field::actionResources | Field::actionParams))
    {
        updateCameraButton();
        updateWarningLabel();
    }

    if (fields.testFlag(Field::actionResources))
    {
        updateLayoutButton();
    }
}

void QnFullscreenCameraActionWidget::updateCameraButton()
{
    using Column = QnBusinessRuleViewModel::Column;

    const bool canUseSource = ((model()->eventType() >= nx::vms::api::userDefinedEvent)
            || (nx::vms::event::requiresCameraResource(model()->eventType())));
    const bool showCamera = !canUseSource || !model()->actionParams().useSource;

    ui->cameraLabel->setVisible(showCamera);
    ui->selectCameraButton->setVisible(showCamera);

    if (showCamera)
    {
        ui->selectCameraButton->setText(FullscreenActionHelper::cameraText(model().data()));
        ui->selectCameraButton->setIcon(FullscreenActionHelper::cameraIcon(model().data()));
    }
}

void QnFullscreenCameraActionWidget::updateLayoutButton()
{
    if (!model())
        return;

    auto button = ui->selectLayoutButton;

    button->setForegroundRole(FullscreenActionHelper::isLayoutValid(model().data())
        ? QPalette::BrightText
        : QPalette::ButtonText);

    button->setText(FullscreenActionHelper::layoutText(model().data()));
    button->setIcon(FullscreenActionHelper::layoutIcon(model().data()));
}

void QnFullscreenCameraActionWidget::updateWarningLabel()
{
    const bool valid = FullscreenActionHelper::cameraExistOnLayouts(model().data());
    ui->layoutWarningWidget->setHidden(valid);

    if (!valid)
    {
        const auto layoutCount = FullscreenActionHelper::layoutIds(model().data()).size();
        if (layoutCount == 1)
        {
            ui->layoutWarningWidget->setText(
                tr("This camera is not currently on the selected layout. "
                   "Action will work if camera is added before action triggers."));
        }
        else
        {
            ui->layoutWarningWidget->setText(
                tr("This camera is not currently on some of the selected layouts. "
                    "Action will work if camera is added before action triggers."));
        }
    }
}

void QnFullscreenCameraActionWidget::openCameraSelectionDialog()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    using namespace nx::vms::client::desktop::node_view;
    auto selectedCameras = FullscreenActionHelper::cameraIds(model().data());
    if (!CameraSelectionDialog::selectCameras<QnFullscreenCameraPolicy>(selectedCameras, this))
        return;

    model()->setActionResourcesRaw(
        FullscreenActionHelper::setCameraIds(model().data(), selectedCameras));

    updateCameraButton();
    updateWarningLabel();
}

void QnFullscreenCameraActionWidget::openLayoutSelectionDialog()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    using namespace nx::vms::client::desktop::node_view;
    auto selection = model()->actionResources();
    if (!MultipleLayoutSelectionDialog::selectLayouts(selection, this))
        return;

    model()->setActionResourcesRaw(FullscreenActionHelper::setLayoutIds(model().data(), selection));

    updateCameraButton();
    updateWarningLabel();
    updateLayoutButton();
}

void QnFullscreenCameraActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    auto params = model()->actionParams();
    params.useSource = ui->useSourceCheckBox->isChecked();
    model()->setActionParams(params);

    updateCameraButton();
    updateWarningLabel();
}
