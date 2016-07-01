#ifndef QN_WOKRBENCH_USER_LAYOUT_COUNT_WATCHER_H
#define QN_WOKRBENCH_USER_LAYOUT_COUNT_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchUserLayoutCountWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchUserLayoutCountWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchUserLayoutCountWatcher();

    int layoutCount() const {
        return m_layoutCount;
    }

signals:
    void layoutCountChanged();

private slots:
    void at_context_userChanged();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource, bool update = true);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource, bool update = true);
    void at_resource_parentIdChanged(const QnResourcePtr &resource, bool update = true);

    void updateLayoutCount();

private:
    QnUuid m_currentUserId;
    QSet<QnResourcePtr> m_layouts;
    int m_layoutCount;
};


#endif // QN_WOKRBENCH_USER_LAYOUT_COUNT_WATCHER_H
