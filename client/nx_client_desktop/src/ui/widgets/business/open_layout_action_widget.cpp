#include "open_layout_action_widget.h"
#include "ui_open_layout_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/models/resource/resource_list_model.h>
#include <utils/common/scoped_value_rollback.h>

#include <nx/vms/event/action_parameters.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {

OpenLayoutActionWidget::OpenLayoutActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::OpenLayoutActionWidget)
{
    ui->setupUi(this);

    m_listModel = new QnResourceListModel(this);

    QnResourceList resources = resourcePool()->getAllResourceByTypeName(QnResourceTypePool::kLayoutTypeId);
    m_listModel->setResources(std::move(resources));
    ui->selectLayoutButton->setModel(m_listModel);
    connect(ui->selectLayoutButton, QnComboboxCurrentIndexChanged,
        this, &OpenLayoutActionWidget::at_layoutSelectionChanged);
    setSubjectsButton(ui->selectUsersButton);
}

OpenLayoutActionWidget::~OpenLayoutActionWidget()
{
}

void OpenLayoutActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->selectLayoutButton);
    setTabOrder(ui->selectLayoutButton, after);
}

void OpenLayoutActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        vms::event::ActionParameters params = model()->actionParams();
        int index = 0;
        for(const auto& res: m_listModel->resources())
        {
            if (res && res->getId() == params.layoutResourceId)
            {
                ui->selectLayoutButton->setCurrentIndex(index);
                break;
            }
            index++;
        }
    }
}

void OpenLayoutActionWidget::at_layoutSelectionChanged(int index)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    vms::event::ActionParameters params = model()->actionParams();

    const QnResourceList& resources = m_listModel->resources();
    if (index >= 0 && index < resources.size())
    {
        auto uuid = resources[index]->getId();
        params.layoutResourceId = uuid;
    }

    model()->setActionParams(params);
}

} // namespace desktop
} // namespace client
} // namespace nx
