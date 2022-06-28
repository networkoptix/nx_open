// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QTabBar>

#include <network/base_system_description.h>
#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class SystemTabBarModel;

class SystemTabBar:
    public QTabBar,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QTabBar;

public:
    SystemTabBar(QWidget* parent = nullptr);
    void setModel(SystemTabBarModel* model);
    void rebuildTabs();

protected:
    virtual QSize tabSizeHint(int index) const override;

private:
    void addClosableTab(const QString& text, const QnSystemDescriptionPtr& systemDescription);
    bool isHomeTab(int index) const;
    int homeTabIndex() const;
    void updateHomeTab();

    void at_currentTabChanged(int index);
    void at_currentSystemChanged(QnSystemDescriptionPtr systemDescription);
    void at_systemDisconnected();

private:
    bool m_updating = false;
    int m_lastTabIndex = 0;
};

} // namespace nx::vms::client::desktop
