#include "fullscreen_camera_action_widget.h"
#include "ui_fullscreen_camera_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/client/desktop/event_rules/helpers/fullscreen_action_helper.h>

#include <nx/client/desktop/ui/event_rules/layout_selection_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <business/business_resource_validation.h>

using namespace nx::client::desktop;

QnFullscreenCameraActionWidget::QnFullscreenCameraActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FullscreenCameraActionWidget)
{
    ui->setupUi(this);
    ui->layoutWarningWidget->setVisible(false);

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
    }

    if (fields.testFlag(Field::actionResources))
    {
        updateLayoutButton();
    }
}

void QnFullscreenCameraActionWidget::updateCameraButton()
{
    using Column = QnBusinessRuleViewModel::Column;

    const bool canUseSource = ((model()->eventType() >= nx::vms::event::userDefinedEvent)
            || (requiresCameraResource(model()->eventType())));
    const bool showCamera = !canUseSource || !model()->actionParams().useSource;

    ui->cameraLabel->setVisible(showCamera);
    ui->selectCameraButton->setVisible(showCamera);

    if (showCamera)
    {
        ui->selectCameraButton->setText(FullscreenActionHelper::cameraText(model().data()));
        ui->selectCameraButton->setIcon(model()->iconForAction());
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

void QnFullscreenCameraActionWidget::openCameraSelectionDialog()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::cameras, this);
    dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnFullscreenCameraPolicy>(this));
    dialog.setSelectedResources(FullscreenActionHelper::cameraIds(model().data()));

    if (dialog.exec() != QDialog::Accepted)
        return;

    model()->setActionResourcesRaw(
        FullscreenActionHelper::setCameraIds(model().data(), dialog.selectedResources()));
    updateCameraButton();
}

void QnFullscreenCameraActionWidget::openLayoutSelectionDialog()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    LayoutSelectionDialog dialog(/*singlePick*/ false, this);

    const auto selectionMode = LayoutSelectionDialog::ModeFull;

    const auto selection = model()->actionResources();

    const auto localLayouts = resourcePool()->getResources<QnLayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return !layout->isShared() && layout->hasFlags(Qn::remote);
        });
    dialog.setLocalLayouts(localLayouts, selection, selectionMode);

    const auto sharedLayouts = resourcePool()->getResources<QnLayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return layout->isShared() && layout->hasFlags(Qn::remote);
        });

    dialog.setSharedLayouts(sharedLayouts, selection);

    if (dialog.exec() != QDialog::Accepted)
        return;

    model()->setActionResourcesRaw(
        FullscreenActionHelper::setLayoutIds(model().data(), dialog.checkedLayouts()));

    // checkWarnings();
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
}
