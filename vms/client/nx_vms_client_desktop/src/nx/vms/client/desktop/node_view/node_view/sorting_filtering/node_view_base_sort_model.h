// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewBaseSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    NodeViewBaseSortModel(QObject* parent = nullptr);

    virtual ~NodeViewBaseSortModel() override;

    virtual void setSourceModel(QAbstractItemModel* model) override;

    bool filteringEnabled() const;
    void setFilteringEnabled(bool enabled);

    QString filterString() const;
    void setFilterString(const QString& string);

    using FilterFunction =
        std::function<bool(const QModelIndex& index, const QString& filterString)>;
    void setFilterFunction(FilterFunction filterFunction);

protected:
    bool nextLessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
