#ifndef QN_RESOURCE_SEARCH_SYNCHRONIZER_H
#define QN_RESOURCE_SEARCH_SYNCHRONIZER_H

#include <QObject>
#include <core/resourcemanagment/resource_criterion.h>

class QnWorkbenchLayout;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizerCriterion;

/**
 * This class performs bidirectional synchronization of 
 * <tt>QnWorkbenchLayout</tt> and <tt>QnResourceSearchProxyModel</tt> instances.
 */
class QnResourceSearchSynchronizer: public QObject {
public:
    QnResourceSearchSynchronizer(QObject *parent = NULL);

    virtual ~QnResourceSearchSynchronizer();

    void setModel(QnResourceSearchProxyModel *model);

    QnResourceSearchProxyModel *model() const {
        return m_model;
    }

    void setLayout(QnWorkbenchLayout *layout);

    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

protected:
    void start();
    void stop();

protected slots:
    void at_layout_aboutToBeDestroyed();
    void at_model_destroyed();

private:
    friend class QnResourceSearchSynchronizerCriterion;

    /** Associated layout. */
    QnWorkbenchLayout *m_layout;

    /** Associated model. */
    QnResourceSearchProxyModel *m_model;

    /** Whether changes should be propagated from layout to model. */
    bool m_submit;

    /** Whether changes should be propagated from model to layout. */
    bool m_update;

    /** Set of resource unique identifiers that were manually added to the current layout. */
    QSet<QString> m_manuallyAdded;
};

#endif // QN_RESOURCE_SEARCH_SYNCHRONIZER_H
