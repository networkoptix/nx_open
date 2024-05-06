// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/analytics/analytics_filter_model.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/analytics_filter_model.h")

namespace nx::analytics::taxonomy { class AbstractEngine; }

namespace nx::vms::client::desktop {

namespace analytics { class TaxonomyManager; }

/**
 * An item model providing a tree of analytics detectable object types available in the system.
 */
class DetectableObjectTypeModel: public ScopedModelOperations<QAbstractItemModel>
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AnalyticsFilterModel*
        sourceModel READ sourceModel CONSTANT)

    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    explicit DetectableObjectTypeModel(
        analytics::taxonomy::AnalyticsFilterModel* filterModel,
        QObject* parent = nullptr);
    virtual ~DetectableObjectTypeModel() override;

    virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    void setLiveTypesExcluded(bool value);
    void setEngine(nx::analytics::taxonomy::AbstractEngine* value);

    analytics::taxonomy::AnalyticsFilterModel* sourceModel() const;

    enum Roles
    {
        NameRole = Qt::DisplayRole, //< QString.
        IdsRole = Qt::UserRole, //< QStringList.
        MainIdRole //< QString.
    };

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
