#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_H

#include <QSortFilterProxyModel>
#include <core/resource/resource_fwd.h>
#include "item_data_role.h"

class QnResourceCriterionGroup;
class QnResourceCriterion;

/**
 * A resource filtering model that uses resource criteria for filtering.
 */
class QnResourceSearchProxyModel: public QSortFilterProxyModel {
    Q_OBJECT;

public:
    explicit QnResourceSearchProxyModel(QObject *parent = 0);

    virtual ~QnResourceSearchProxyModel();

    void addCriterion(QnResourceCriterion *criterion);

    bool removeCriterion(QnResourceCriterion *criterion);

    bool replaceCriterion(QnResourceCriterion *from, QnResourceCriterion *to);

    const QnResourceCriterionGroup *criteria() {
        return m_criterionGroup.data();
    }

    QnResourcePtr resource(const QModelIndex &index) const;

signals:
    void criteriaChanged();

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
    QScopedPointer<QnResourceCriterionGroup> m_criterionGroup;
    QObject *m_invalidateListener;
};

#endif // QN_RESOURCE_SEARCH_PROXY_MODEL_H
