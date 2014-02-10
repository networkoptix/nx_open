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

    void watchLayout(QnWorkbenchLayout *layout);
    void removeLayout(QnWorkbenchLayout *layout);
    QSet<QnWorkbenchLayout*> watchedLayouts() const;

private slots:
    void at_renderWatcher_displayingChanged(QnResourceWidget *widget);
    void at_resourceWidget_aspectRatioChanged();
    void at_watchedLayout_destroyed();
    void at_watchedLayout_itemAdded();
    void at_watchedLayout_itemRemoved();

private:
    void setLayoutAspectRatio(QnWorkbenchLayout *layout, qreal aspectRatio);

private:
    QnWorkbenchRenderWatcher *m_renderWatcher;
    QSet<QnWorkbenchLayout*> m_watchedLayouts;
};

#endif // QN_WORKBENCH_MEDIA_WIDGET_WATCHER_H
