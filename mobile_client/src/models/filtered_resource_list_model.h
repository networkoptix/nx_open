#ifndef QNFILTEREDRESOURCELISTMODEL_H
#define QNFILTEREDRESOURCELISTMODEL_H

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnFilteredResourceListModel : public Connective<QAbstractListModel> {
    Q_OBJECT

    typedef Connective<QAbstractListModel> base_type;

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

protected:
    void resetResourcesInternal();

private:
    QList<QnResourcePtr> m_resources;
};

#endif // QNFILTEREDRESOURCELISTMODEL_H
