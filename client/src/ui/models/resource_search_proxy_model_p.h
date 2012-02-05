#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_PRIVATE_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_PRIVATE_H

#include "resource_search_proxy_model.h"
#include "resource_model_p.h"

class QnResourceSearchProxyModelPrivate
{
    Q_DECLARE_PUBLIC(QnResourceSearchProxyModel)

public:
    QnResourceSearchProxyModelPrivate();

    bool matchesFilters(const QRegExp filters[], Node *node, int source_row, const QModelIndex &source_parent) const;

    static QString normalizedFilterString(const QString &str);
    static void buildFilters(const QSet<QString> parts[], QRegExp *filters);
    void parseFilterString();

private:
    QnResourceSearchProxyModel *q_ptr;

    QString parsedFilterString;
    uint flagsFilter;

    enum FilterCategory {
        Text, Name, Tags, Id,
        NumFilterCategories
    };

    QRegExp negfilters[NumFilterCategories];
    QRegExp filters[NumFilterCategories];
};

#endif // QN_RESOURCE_SEARCH_PROXY_MODEL_PRIVATE_H
