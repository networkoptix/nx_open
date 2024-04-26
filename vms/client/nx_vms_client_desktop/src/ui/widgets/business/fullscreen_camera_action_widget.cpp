// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fullscreen_camera_action_widget.h"
#include "ui_fullscreen_camera_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <business/business_resource_validation.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/client/desktop/rules/helpers/fullscreen_action_helper.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>

using namespace nx::vms::client::desktop;
using namespace std::chrono;

QnFullscreenCameraActionWidget::QnFullscreenCameraActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::FullscreenCameraActionWidget)
{
    ui->setupUi(this);
    ui->layoutWarningWidget->setVisible(false);
    setWarningStyle(ui->layoutWarningWidget);

    connect(ui->selectCameraButton, &QPushButton::clicked,
        this, &QnFullscreenCameraActionWidget::openCameraSelectionDialog);

    connect(ui->selectLayoutButton, &QPushButton::clicked,
        this, &QnFullscreenCameraActionWidget::openLayoutSelectionDialog);

    connect(ui->rewindForWidget, &RewindForWidget::valueEdited, this,
        [this]()
        {
            const bool isValidState = model() && !m_updating;
            if (!NX_ASSERT(isValidState))
                return;

            QScopedValueRollback<bool> guard(m_updating, true);
            auto params = model()->actionParams();
            params.recordBeforeMs = milliseconds(ui->rewindForWidget->value()).count();
            model()->setActionParams(params);
        });
}

QnFullscreenCameraActionWidget::~QnFullscreenCameraActionWidget()
{
}

void QnFullscreenCameraActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectCameraButton);
    setTabOrder(ui->selectCameraButton, ui->selectLayoutButton);
    setTabOrder(ui->selectLayoutButton, after);
}

void QnFullscreenCameraActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        ui->rewindForWidget->setValue(duration_cast<seconds>(
            milliseconds(model()->actionParams().recordBeforeMs)));
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
    model()->setActionParams(params);

    updateCameraButton();
    updateWarningLabel();
}
