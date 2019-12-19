#include "select_servers_delegate_editor_button.h"

namespace nx::vms::client::desktop {
namespace ui {

SelectServersDialogButton::SelectServersDialogButton(QWidget* parent):
    base_type(parent)
{
    m_selectServersFunction =
        [this](QnUuidSet& serverIds)
        {
            return ServerSelectionDialog::selectServers(serverIds,
                ServerSelectionDialog::ServerFilter(), QString(), this);
        };
}

SelectServersDialogButton::~SelectServersDialogButton()
{
}

void SelectServersDialogButton::handleButtonClicked()
{
    if (!NX_ASSERT(m_selectServersFunction))
        return;

    auto serverIds = getResources();
    if (!m_selectServersFunction(serverIds))
        return;

    setResources(serverIds);
    emit commit();
}

} // namespace ui
} // namespace nx::vms::client::desktop
