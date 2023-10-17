// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QAbstractListModel>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Base model for all Right Panel data models. Provides action activation via setData.
 */
class AbstractEventListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    explicit AbstractEventListModel(WindowContext* context, QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

protected:
    bool isValid(const QModelIndex& index) const;
    virtual QString timestampText(std::chrono::microseconds timestamp) const;

    virtual bool defaultAction(const QModelIndex& index);
    virtual bool activateLink(const QModelIndex& index, const QString& link);
};

} // namespace nx::vms::client::desktop
