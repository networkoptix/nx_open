#ifndef QN_RESOURCE_LIST_MODEL_H
#define QN_RESOURCE_LIST_MODEL_H

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

#include "item_data_role.h"

class QnResourceListModel: public QAbstractListModel {
    Q_OBJECT;

public:
    QnResourceListModel(QObject *parent = NULL);
    virtual ~QnResourceListModel();

    const QnResourceList &resouces() const;
    void setResources(const QnResourceList &resouces);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private slots:
    void at_resource_resourceChanged(const QnResourcePtr &resource);
    void at_resource_resourceChanged();

private:
    QnResourceList m_resources;
};


#endif // QN_RESOURCE_LIST_MODEL_H

