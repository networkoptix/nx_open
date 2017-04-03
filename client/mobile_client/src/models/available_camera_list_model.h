#pragma once

#include <QtCore/QAbstractListModel>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <utils/common/id.h>

class QnAvailableCameraListModelPrivate;
class QnAvailableCameraListModel:
    public Connective<QAbstractListModel>,
    public QnConnectionContextAware
{
    Q_OBJECT

    typedef Connective<QAbstractListModel> base_type;

public:
    QnAvailableCameraListModel(QObject* parent = nullptr);
    ~QnAvailableCameraListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void refreshResource(const QnResourcePtr& resource, int role = -1);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QModelIndex indexByResourceId(const QnUuid& resourceId) const;

protected:
    virtual bool filterAcceptsResource(const QnResourcePtr& resource) const;

private:
    QScopedPointer<QnAvailableCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCameraListModel)
};

