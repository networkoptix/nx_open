#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_H

#include <QtCore/QMetaType>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_criterion.h>


/**
 * A resource filtering model that uses resource criteria for filtering.
 */
class QnResourceSearchProxyModel: public QSortFilterProxyModel {
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

public:
    explicit QnResourceSearchProxyModel(QObject *parent = 0);

    virtual ~QnResourceSearchProxyModel();

    void addCriterion(const QnResourceCriterion &criterion);

    bool removeCriterion(const QnResourceCriterion &criterion);

    void clearCriteria();

    const QnResourceCriterionGroup &criteria() {
        return m_criterionGroup;
    }

    QnResourcePtr resource(const QModelIndex &index) const;

protected:

    // --------------------------------------------------------------
    // Add a override function for lessThan to achieve customization
    // --------------------------------------------------------------
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

signals:
    void criteriaChanged();

    /**
     * This signal is emitted when the tree prepares to start a recursive operation
     * that may lead to a lot of dataChanged signals emitting.
     */
    void beforeRecursiveOperation();

    /**
     * This signal is emitted when the tree ends a recursive operation.
     */
    void afterRecursiveOperation();

public slots:
    void invalidateFilter();

    /**
     * Performs delayed invalidation of the filter. 
     * 
     * The filter will be invalidated only once even if this function was 
     * called several times.
     */
    void invalidateFilterLater();

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QnResourceCriterionGroup m_criterionGroup;
    bool m_invalidating;
};

Q_DECLARE_METATYPE(QnResourceSearchProxyModel *)


#endif // QN_RESOURCE_SEARCH_PROXY_MODEL_H
