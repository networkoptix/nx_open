// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context_aware.h>

class QnLayoutTabBar;
class QnMainWindowTitleBarWidgetPrivate;
namespace nx::vms::client::desktop { class ToolButton; }

class QnMainWindowTitleBarWidget: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged)

    using base_type = QWidget;

public:
    explicit QnMainWindowTitleBarWidget(
        QnWorkbenchContext* context,
        QWidget* parent = nullptr);

    ~QnMainWindowTitleBarWidget();

    int y() const;
    void setY(int y);

    QnLayoutTabBar* tabBar() const;

    /**
    * Sets tab bar and "add"/"tabs list menu" buttons visible (or not)
    */
    void setTabBarStuffVisible(bool visible);
    void activateHomeTab();
    void activatePreviousTab();

signals:
    void yChanged(int newY);
    void geometryChanged();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

private:
    nx::vms::client::desktop::ToolButton* newActionButton(
        nx::vms::client::desktop::ui::action::IDType actionId,
        int helpTopicId = Qn::Empty_Help,
        const QSize& fixedSize = QSize());

    nx::vms::client::desktop::ToolButton* newActionButton(
        nx::vms::client::desktop::ui::action::IDType actionId,
        const QSize& fixedSize = QSize());
    void initMultiSystemTabBar();
    void initLayoutsOnlyTabBar();

private:
    Q_DECLARE_PRIVATE(QnMainWindowTitleBarWidget)

    QScopedPointer<QnMainWindowTitleBarWidgetPrivate> d_ptr;
};
