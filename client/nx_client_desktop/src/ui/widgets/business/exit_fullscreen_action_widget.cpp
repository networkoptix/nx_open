#include "exit_fullscreen_action_widget.h"
#include "ui_exit_fullscreen_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/client/desktop/event_rules/helpers/exit_fullscreen_action_helper.h>

#include <nx/client/desktop/ui/event_rules/layout_selection_dialog.h>

using namespace nx::client::desktop;

QnExitFullscreenActionWidget::QnExitFullscreenActionWidget(QWidget* parent):
    base_type(parent),
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
        ExitFullscreenActionHelper::setLayoutIds(model().data(), dialog.checkedLayouts()));

    updateLayoutButton();
}

