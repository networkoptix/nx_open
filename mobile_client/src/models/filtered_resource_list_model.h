#ifndef QNFILTEREDRESOURCELISTMODEL_H
#define QNFILTEREDRESOURCELISTMODEL_H

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

class QnFilteredResourceListModel : public QAbstractListModel {
    Q_OBJECT
public:
    QnFilteredResourceListModel(QObject *parent = 0);
    ~QnFilteredResourceListModel();

    void resetResources();

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const;

    void refreshResource(const QnResourcePtr &resource, int role = -1);

protected:
    virtual bool filterAcceptsResource(const QnResourcePtr &resource) const = 0;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    void resetResourcesInternal();

private:
    QList<QnResourcePtr> m_resources;
};

#endif // QNFILTEREDRESOURCELISTMODEL_H
