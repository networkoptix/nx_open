#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnServerUpdatesWidget;
}

class QnServerUpdatesWidget : public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnServerUpdatesWidget(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnServerUpdatesWidget();

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;
};

#endif // SERVER_UPDATES_WIDGET_H
