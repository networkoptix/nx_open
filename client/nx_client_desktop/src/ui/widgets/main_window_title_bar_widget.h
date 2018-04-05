#pragma once

#include <QtWidgets/QWidget>

#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context_aware.h>

class QnLayoutTabBar;
class QnMainWindowTitleBarWidgetPrivate;
namespace nx { namespace client { namespace desktop { class ToolButton; }}}

class QnMainWindowTitleBarWidget: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

   using base_type = QWidget;

public:
    explicit QnMainWindowTitleBarWidget(
        QWidget* parent = nullptr,
        QnWorkbenchContext* context = nullptr);

    ~QnMainWindowTitleBarWidget();

    QnLayoutTabBar* tabBar() const;

    /**
    * Sets tab bar and "add"/"tabs list menu" buttons visible (or not)
    */
    void setTabBarStuffVisible(bool visible);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

private:
    nx::client::desktop::ToolButton* newActionButton(
        nx::client::desktop::ui::action::IDType actionId,
        int helpTopicId = Qn::Empty_Help,
        const QSize& fixedSize = QSize());

    nx::client::desktop::ToolButton* newActionButton(
        nx::client::desktop::ui::action::IDType actionId,
        const QSize& fixedSize = QSize());

private:
    Q_DECLARE_PRIVATE(QnMainWindowTitleBarWidget)

    QScopedPointer<QnMainWindowTitleBarWidgetPrivate> d_ptr;
};
