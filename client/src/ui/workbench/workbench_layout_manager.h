#ifndef QN_WORKBENCH_LAYOUT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_MANAGER_H

#include <QObject>
#include <core/resource/layout_item_data.h>

class QnWorkbenchContext;

namespace detail {
    class WorkbenchSynchronizerReplyProcessor: public QObject {
        Q_OBJECT

    public:
        WorkbenchSynchronizerReplyProcessor(QnWorkbenchSynchronizer *synchronizer, const QnLayoutResourcePtr &resource): 
            m_synchronizer(synchronizer),
            m_resource(resource)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);

    private:
        QWeakPointer<QnWorkbenchSynchronizer> m_synchronizer;
        QnLayoutResourcePtr m_resource;
    };

    class LayoutData {
    public:
        LayoutData(const QnLayoutResourcePtr &resource):
            items(resource->getItems()),
            name(resource->getName()),
            changed(false)
        {}

        LayoutData() {}

        QnLayoutItemDataMap items;
        QString name;
        bool changed;
    };

} // namespace detail


class QnWorkbenchLayoutManager: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchLayoutManager(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutManager();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    void setContext(QnWorkbenchContext *context);

    void save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot);

    void restore(const QnLayoutResourcePtr &resource);

    bool isChanged(const QnLayoutResourcePtr &resource);

    bool isLocal(const QnLayoutResourcePtr &resource);

protected:
    void start();
    void stop();

    QnLayoutResourceList poolLayoutResources() const;

    void setChanged(const QnLayoutResourcePtr &resource, bool changed);
    const detail::LayoutData &savedState(const QnLayoutResourcePtr &resource);
    void setSavedState(const QnLayoutResourcePtr &resource, const detail::LayoutData &state);
    void removeSavedState(const QnLayoutResourcePtr &resource);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnWorkbenchContext *m_context;
};


#endif // QN_WORKBENCH_LAYOUT_MANAGER_H
