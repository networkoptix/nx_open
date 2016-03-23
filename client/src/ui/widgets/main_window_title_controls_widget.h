#pragma once

#include <QtWidgets/QWidget>

#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context_aware.h>

class QnMainWindowTitleControlsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QWidget base_type;
public:
    explicit QnMainWindowTitleControlsWidget(QWidget* parent = nullptr, QnWorkbenchContext* context = nullptr);

    static QToolButton* newActionButton(QAction *action, bool popup = false, qreal sizeMultiplier = 1.0, int helpTopicId = Qn::Empty_Help);
};
