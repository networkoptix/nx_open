#ifndef QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H
#define QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchRenderWatcher;

class QnWorkbenchLayoutAspectRatioWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchLayoutAspectRatioWatcher(QObject *parent = 0);
    ~QnWorkbenchLayoutAspectRatioWatcher();

private slots:
    void at_renderWatcher_displayingChanged(QnResourceWidget *widget);
    void at_resourceWidget_aspectRatioChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_watchedLayout_cellAspectRatioChanged();

private:
    void watchCurrentLayout();
    void unwatchCurrentLayout();

private:
    QnWorkbenchRenderWatcher *m_renderWatcher;
    QnWorkbenchLayout *m_watchedLayout;
    QSet<QnResourceWidget*> m_watchedWidgets;
};

#endif // QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H
