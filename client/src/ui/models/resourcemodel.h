#ifndef RESOURCEMODEL_H
#define RESOURCEMODEL_H

#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"

class ResourceModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit ResourceModel(QObject *parent = 0);
    ~ResourceModel();

private Q_SLOTS:
    void addResource(QnResourcePtr resource);
    void removeResource(QnResourcePtr resource);

private:
    QStandardItem *itemFromResourceId(uint id) const;
};

#endif // RESOURCEMODEL_H
