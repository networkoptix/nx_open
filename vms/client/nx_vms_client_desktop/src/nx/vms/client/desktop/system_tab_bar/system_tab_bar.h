// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QTabBar>

#include <nx/vms/client/core/system_finder/system_description_fwd.h>
#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

struct MainWindowTitleBarState;
class MainWindowTitleBarStateStore;
class SystemTabBarStateHandler;
struct LogonData;

class SystemTabBar:
    public QTabBar,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QTabBar;
    using Store = MainWindowTitleBarStateStore;
    using State = MainWindowTitleBarState;
    using StateHandler = SystemTabBarStateHandler;

public:
    SystemTabBar(QWidget* parent = nullptr);
    void setStateStore(QSharedPointer<Store> store, QSharedPointer<StateHandler> stateHandler);
    void rebuildTabs();
    bool isUpdating() const;
    bool active() const;

protected:
    virtual QSize tabSizeHint(int index) const override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void contextMenuEvent(QContextMenuEvent* event) override;
    virtual void initStyleOption(QStyleOptionTab* option, int tabIndex) const override;

private slots:
    void at_currentChanged(int index);
    void at_tabMoved(int from, int to);

private:
    void insertClosableTab(int index, const core::SystemDescriptionPtr& systemDescription);
    void disconnectFromSystem(const nx::Uuid& localId);

private:
    bool m_updating = false;
    QSharedPointer<Store> m_store;
    QSharedPointer<StateHandler> m_stateHandler;
};

} // namespace nx::vms::client::desktop
