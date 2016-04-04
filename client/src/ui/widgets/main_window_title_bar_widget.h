#pragma once

#include <QtWidgets/QWidget>

#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context_aware.h>

class QnLayoutTabBar;
class QnMainWindowTitleBarWidgetPrivate;

class QnMainWindowTitleBarWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QWidget base_type;

public:
    explicit QnMainWindowTitleBarWidget(
            QWidget* parent = nullptr,
            QnWorkbenchContext* context = nullptr);

    ~QnMainWindowTitleBarWidget();

    QnLayoutTabBar* tabBar() const;

    static QToolButton* newActionButton(
            QAction* action,
            bool popup = false,
            qreal sizeMultiplier = 1.0,
            int helpTopicId = Qn::Empty_Help);

protected:
    virtual bool event(QEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnMainWindowTitleBarWidget)

    QScopedPointer<QnMainWindowTitleBarWidgetPrivate> d_ptr;
};
