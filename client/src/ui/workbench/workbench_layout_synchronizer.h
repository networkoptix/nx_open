#ifndef QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
#define QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnLayoutItemData;

/**
 * This class performs bidirectional synchronization of instances of 
 * <tt>QnWorkbenchLayout</tt> and <tt>QnLayoutResource</tt>.
 */
class QnWorkbenchLayoutSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchLayoutSynchronizer(QnWorkbenchLayout *layout, const QnLayoutResourcePtr &resource, QObject *parent);

    virtual ~QnWorkbenchLayoutSynchronizer();

    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    const QnLayoutResourcePtr &resource() const {
        return m_resource;
    }

    bool isAutoDeleting() const {
        return m_autoDeleting;
    }

    void setAutoDeleting(bool autoDeleting);

    static QnWorkbenchLayoutSynchronizer *instance(QnWorkbenchLayout *layout);

    static QnWorkbenchLayoutSynchronizer *instance(const QnLayoutResourcePtr &resource);

public slots:
    void update();
    void submit();
    void autoDeleteLater();

protected:
    void clearLayout();
    void clearResource();
    
    void initialize();
    void deinitialize();

protected slots:
    void autoDelete();

    void at_resource_resourceChanged();
    void at_resource_nameChanged();
    void at_resource_itemAdded(const QnLayoutItemData &itemData);
    void at_resource_itemRemoved(const QnLayoutItemData &itemData);

    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_nameChanged();
    void at_layout_aboutToBeDestroyed();

private:
    /** Associated workbench layout. */
    QnWorkbenchLayout *m_layout;

    /** Associated layout resource. */
    QnLayoutResourcePtr m_resource;

    /** Whether changes should be propagated from resource to layout. */
    bool m_update;

    /** Whether changes should be propagated from layout to resource. */
    bool m_submit;

    /** Whether this layout synchronizer should delete itself once it is not functional anymore. */
    bool m_autoDeleting;
};

#endif // QN_WORKBENCH_LAYOUT_SYNCHRONIZER_H
