// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>
#include <utils/common/id.h>

class QnAvailableCameraListModelPrivate;
class QnAvailableCameraListModel:
    public QAbstractListModel,
    public nx::vms::client::mobile::CurrentSystemContextAware
{
    Q_OBJECT

    typedef QAbstractListModel base_type;

public:
    QnAvailableCameraListModel(QObject* parent = nullptr);
    ~QnAvailableCameraListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void refreshResource(const QnResourcePtr& resource, int role = -1);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QModelIndex indexByResourceId(const nx::Uuid& resourceId) const;

protected:
    virtual bool filterAcceptsResource(const QnResourcePtr& resource) const;

private:
    QScopedPointer<QnAvailableCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCameraListModel)
};
