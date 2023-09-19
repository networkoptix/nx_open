// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exit_fullscreen_action_widget.h"
#include "ui_exit_fullscreen_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/client/desktop/rules/helpers/exit_fullscreen_action_helper.h>
#include <nx/vms/event/action_parameters.h>

using namespace nx::vms::client::desktop;

QnExitFullscreenActionWidget::QnExitFullscreenActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::ExitFullscreenActionWidget)
{
    ui->setupUi(this);

    connect(ui->selectLayoutButton, &QPushButton::clicked,
        this, &QnExitFullscreenActionWidget::openLayoutSelectionDialog);
}

QnExitFullscreenActionWidget::~QnExitFullscreenActionWidget()
{
}

void QnExitFullscreenActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectLayoutButton);
    setTabOrder(ui->selectLayoutButton, after);
}

void QnExitFullscreenActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::actionResources))
    {
        updateLayoutButton();
    }
}

void QnExitFullscreenActionWidget::updateLayoutButton()
{
    if (!model())
        return;

    auto button = ui->selectLayoutButton;

    button->setForegroundRole(ExitFullscreenActionHelper::isLayoutValid(model().data())
        ? QPalette::BrightText
        : QPalette::ButtonText);

    button->setText(ExitFullscreenActionHelper::layoutText(model().data()));
    button->setIcon(ExitFullscreenActionHelper::layoutIcon(model().data()));
}

void QnExitFullscreenActionWidget::openLayoutSelectionDialog()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    auto selection = model()->actionResources();
    if (!MultipleLayoutSelectionDialog::selectLayouts(selection, this))
        return;

    model()->setActionResourcesRaw(
        ExitFullscreenActionHelper::setLayoutIds(model().data(), selection));

    updateLayoutButton();
}
