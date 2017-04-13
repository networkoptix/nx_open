#ifndef WORKBENCH_PTZ_DIALOG_WATCHER_H
#define WORKBENCH_PTZ_DIALOG_WATCHER_H

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;

class QnWorkbenchPtzDialogWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchPtzDialogWatcher(QObject *parent = NULL);

private slots:
    void at_workbench_currentLayoutChanged();
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_currentLayout_itemRemoved(QnWorkbenchItem *item);

private:
    void closePtzManageDialog(QnWorkbenchItem *item = NULL);
};

#endif // WORKBENCH_PTZ_DIALOG_WATCHER_H
