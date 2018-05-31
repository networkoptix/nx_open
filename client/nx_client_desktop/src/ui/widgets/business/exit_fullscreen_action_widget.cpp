#include "exit_fullscreen_action_widget.h"
#include "ui_exit_fullscreen_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/event/action_parameters.h>

#include <utils/common/scoped_value_rollback.h>

QnExitFullscreenActionWidget::QnExitFullscreenActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExitFullscreenActionWidget)
{
    ui->setupUi(this);
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
        const auto resources = model()->actionResources();

    }
}

void QnExitFullscreenActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    auto params = model()->actionParams();

    model()->setActionParams(params);
}
