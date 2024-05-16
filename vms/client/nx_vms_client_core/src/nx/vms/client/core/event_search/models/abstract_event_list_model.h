// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QAbstractListModel>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

/**
 * Base model for all Event Search data models. Provides action activation via setData.
 */
class NX_VMS_CLIENT_CORE_API AbstractEventListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    explicit AbstractEventListModel(
        SystemContext* context,
        QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QHash<int, QByteArray> roleNames() const override;

protected:
    bool isValid(const QModelIndex& index) const;

    virtual bool defaultAction(const QModelIndex& index);
    virtual bool activateLink(const QModelIndex& index, const QString& link);
};

} // namespace nx::vms::client::core
