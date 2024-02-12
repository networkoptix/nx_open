// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <ui/delegates/select_resources_delegate_editor_button.h>

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
            [this](UuidSet& serverIds)
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
    std::function<bool(UuidSet& serverIds)> m_selectServersFunction;
};

} // namespace ui
} // namespace nx::vms::client::desktop
