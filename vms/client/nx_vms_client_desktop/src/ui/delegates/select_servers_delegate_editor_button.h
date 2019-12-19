#pragma once

#include <ui/delegates/select_resources_delegate_editor_button.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>

namespace nx::vms::client::desktop {
namespace ui {

class SelectServersDialogButton: public QnSelectResourcesDialogButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesDialogButton;

public:
    explicit SelectServersDialogButton(QWidget* parent = nullptr);
    virtual ~SelectServersDialogButton() override;

    template<typename ServerResourcePolicy>
    void setResourcePolicy()
    {
        m_selectServersFunction =
            [this](QnUuidSet& serverIds)
            {
                return ServerSelectionDialog::selectServers(
                    serverIds,
                    ServerResourcePolicy::isServerValid,
                    ServerResourcePolicy::infoText(),
                    this);
            };
    }

protected:
    virtual void handleButtonClicked() override;

private:
    std::function<bool(QnUuidSet& serverIds)> m_selectServersFunction;
};

} // namespace ui
} // namespace nx::vms::client::desktop
