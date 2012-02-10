#ifndef QN_RESOURCE_SEARCH_PROXY_MODEL_H
#define QN_RESOURCE_SEARCH_PROXY_MODEL_H

#include <QSortFilterProxyModel>
#include <core/resource/resource.h>

class QnResourceSearchProxyModelPrivate;

class QnResourceFilter;

class QnResourceSearchProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit QnResourceSearchProxyModel(QObject *parent = 0);
    virtual ~QnResourceSearchProxyModel();

    /*QnResourcePtr resourceFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromResource(const QnResourcePtr &resource) const;*/

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString parsedFilterString;
    uint flagsFilter;

    enum FilterCategory {
        Text, 
        Name, 
        Tags, 
        Id,
        NumFilterCategories
    };

    QRegExp negfilters[NumFilterCategories];
    QRegExp filters[NumFilterCategories];
};

#endif // QN_RESOURCE_SEARCH_PROXY_MODEL_H
