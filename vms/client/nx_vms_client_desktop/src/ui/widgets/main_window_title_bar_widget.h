// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnLayoutTabBar;
class QnMainWindowTitleBarWidgetPrivate;

namespace nx::vms::client::desktop {
class ToolButton;
class MainWindowTitleBarStateStore;
} // namespace nx::vms::client::desktop

class QnMainWindowTitleBarWidget:
    public QWidget,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged)

    using base_type = QWidget;
    using Store = nx::vms::client::desktop::MainWindowTitleBarStateStore;

public:
    static constexpr int kSystemBarHeight = 32;
    static constexpr int kLayoutBarHeight = 24;
    static constexpr int kFullTitleBarHeight = kSystemBarHeight + kLayoutBarHeight;

    explicit QnMainWindowTitleBarWidget(
        nx::vms::client::desktop::WindowContext* windowContext,
        QWidget* parent = nullptr);
    virtual ~QnMainWindowTitleBarWidget() override;

    int y() const;
    void setY(int y);

    QnLayoutTabBar* tabBar() const;
    void setStateStore(QSharedPointer<Store> store);
    QSharedPointer<Store> stateStore() const;

    /**
    * Sets tab bar and "add"/"tabs list menu" buttons visible (or not)
    */
    void setTabBarStuffVisible(bool visible);
    void setHomeTabActive(bool isActive);
    bool isExpanded() const;
    void collapse();
    void expand();
    bool isSystemTabBarUpdating() const;

signals:
    void yChanged(int newY);
    void geometryChanged();
    void toggled(bool expanded);
    void homeTabActivationChanged(bool isActive);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

private:
    nx::vms::client::desktop::ToolButton* newActionButton(
        nx::vms::client::desktop::menu::IDType actionId,
        const QIcon& icon,
        int helpTopicId = nx::vms::client::desktop::HelpTopic::Id::Empty);

    nx::vms::client::desktop::ToolButton* newActionButton(
        nx::vms::client::desktop::menu::IDType actionId,
        int helpTopicId = nx::vms::client::desktop::HelpTopic::Id::Empty,
        const QSize& fixedSize = QSize());

    nx::vms::client::desktop::ToolButton* newActionButton(
        nx::vms::client::desktop::menu::IDType actionId,
        const QSize& fixedSize = QSize());

    /** Creates Screen Recording indicator. Returns nullptr if the recording is not avaliable. */
    QWidget* newRecordingIndicator(const QSize& fixedSize = QSize());

    void initMultiSystemTabBar();
    void initLayoutsOnlyTabBar();

private:
    Q_DECLARE_PRIVATE(QnMainWindowTitleBarWidget)

    QScopedPointer<QnMainWindowTitleBarWidgetPrivate> d_ptr;
};

class QnMainWindowTitleBarResizerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnMainWindowTitleBarResizerWidget(QnMainWindowTitleBarWidget* parent);

signals:
    void collapsed();
    void expanded();

protected:
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;

private:
    bool m_isBeingMoved = false;
};
