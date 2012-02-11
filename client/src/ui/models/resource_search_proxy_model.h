#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_H

#include <QSortFilterProxyModel>
#include <core/resource/resource.h>
#include <core/resourcemanagment/resource_criterion.h>

/**
 * A resource filtering model that uses resource criteria for filtering.
 */
class QnResourceSearchProxyModel: public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit QnResourceSearchProxyModel(QObject *parent = 0);

    virtual ~QnResourceSearchProxyModel();

    void addCriterion(const QnResourceCriterion &criterion);

    bool removeCriterion(const QnResourceCriterion &criterion);

    bool replaceCriterion(const QnResourceCriterion &from, const QnResourceCriterion &to);

public slots:
    void invalidateFilter() {
        QSortFilterProxyModel::invalidateFilter();
    }

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QnResourceCriterionGroup m_criterionGroup;
};

#endif // QN_RESOURCE_SEARCH_PROXY_MODEL_H
