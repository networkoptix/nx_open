// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>

namespace nx::vms::client::core {

class SystemContext;

/**
 * Base model for all Event Search data models. Provides action activation via setData.
 */
class NX_VMS_CLIENT_CORE_API AbstractEventListModel:
    public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    explicit AbstractEventListModel(QObject* parent = nullptr);
    virtual ~AbstractEventListModel();

    SystemContext* systemContext() const;
    virtual void setSystemContext(SystemContext* systemContext);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QHash<int, QByteArray> roleNames() const override;

protected:
    bool isValid(const QModelIndex& index) const;

    virtual bool defaultAction(const QModelIndex& index);
    virtual bool activateLink(const QModelIndex& index, const QString& link);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
