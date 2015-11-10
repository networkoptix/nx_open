#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnAvailableCameraListModel : public Connective<QAbstractListModel> {
    Q_OBJECT

    typedef Connective<QAbstractListModel> base_type;

public:
    QnAvailableCameraListModel(QObject *parent = 0);
    ~QnAvailableCameraListModel();

    void resetResources();

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const;

    void refreshResource(const QnResourcePtr &resource, int role = -1);

protected:
    virtual bool filterAcceptsResource(const QnResourcePtr &resource) const;

private slots:
    void at_watcher_cameraAdded(const QnResourcePtr &resource);
    void at_watcher_cameraRemoved(const QnResourcePtr &resource);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

protected:
    void resetResourcesInternal();

private:
    QList<QnResourcePtr> m_resources;
};

