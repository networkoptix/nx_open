#ifndef QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H
#define QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchRenderWatcher;

class QnWorkbenchMediaWidgetWatcher : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchMediaWidgetWatcher(QObject *parent = 0);
    ~QnWorkbenchMediaWidgetWatcher();

    void adjustLayoutAspectRatio(QnWorkbenchLayout *layout);

signals:
    void widgetAspectRatioChanged(QnResourceWidget *widget, qreal aspectRatio);

private slots:
    void widgetDisplayingChanged(QnResourceWidget *widget);
    void widgetAspectRatioChanged();

private:
    void setLayoutAspectRatio(QnWorkbenchLayout *layout, qreal aspectRatio);

private:
    QnWorkbenchRenderWatcher *m_renderWatcher;
    QSet<QnWorkbenchLayout*> m_watchedLayouts;
};

#endif // QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H
