// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QTabBar>

#include <network/base_system_description.h>
#include <nx/vms/client/desktop/common/widgets/tool_button.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

struct MainWindowTitleBarState;
class MainWindowTitleBarStateStore;
class SystemTabBarStateHandler;

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
    void activateHomeTab();
    bool isHomeTabActive() const;
    bool isUpdating() const;

protected:
    virtual QSize tabSizeHint(int index) const override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

private:
    void insertClosableTab(int index,
        const QString& text,
        const QnSystemDescriptionPtr& systemDescription);
    bool isHomeTab(int index) const;
    int homeTabIndex() const;
    void updateHomeTabView();
    QPixmap imageData(int tabIndex) const;

private:
    bool m_updating = false;
    QSharedPointer<Store> m_store;
    QSharedPointer<StateHandler> m_stateHandler;
    QPoint m_dragStartPosition;
};

} // namespace nx::vms::client::desktop
