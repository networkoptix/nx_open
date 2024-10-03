// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <ui/widgets/system_tab_bar_state_handler.h>

namespace nx::vms::client::desktop {

class CloseTabButton;

class HomeTabButton: public ToolButton
{
    Q_OBJECT

    using base_type = ToolButton;
    using Store = MainWindowTitleBarStateStore;

public:
    HomeTabButton(QWidget* parent = nullptr);
    void setStateStore(QSharedPointer<Store> store);

private slots:
    void updateAppearance();
    void on_clicked();

private:
    QSharedPointer<Store> m_store;
    CloseTabButton* m_closeButton;
};

} // namespace nx::vms::client::desktop
