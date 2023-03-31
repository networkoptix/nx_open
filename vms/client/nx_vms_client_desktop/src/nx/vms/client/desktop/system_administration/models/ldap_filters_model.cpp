// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_filters_model.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

struct LdapFiltersModel::Private
{
    QList<LdapFilter> filters;

    bool isRowValid(int row) const
    {
        return row >= 0 && (qsizetype) row < filters.size();
    }
};

LdapFiltersModel::LdapFiltersModel(QObject* parent):
    base_type(parent),
    d(new Private{})
{
}

LdapFiltersModel::~LdapFiltersModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

int LdapFiltersModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->filters.size();
}

QHash<int, QByteArray> LdapFiltersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[BaseRole] = "base";
    roles[FilterRole] = "filter";
    return roles;
}

QVariant LdapFiltersModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !d->isRowValid(index.row()))
        return {};

    const auto& filter = d->filters[index.row()];

    switch (role)
    {
        case NameRole:
            return filter.name;

        case BaseRole:
            return filter.base;

        case FilterRole:
            return filter.filter;

        default:
            return {};
    }
}

void LdapFiltersModel::addFilter(const QString& name, const QString& base, const QString& filter)
{
    beginInsertRows({}, 0, 0);
    d->filters.insert(0, LdapFilter{
        .name = name,
        .base = base,
        .filter = filter
    });
    endInsertRows();

    emit filtersChanged();
}

void LdapFiltersModel::removeFilter(int row)
{
    if (!d->isRowValid(row))
        return;

    beginRemoveRows({}, row, row);
    d->filters.removeAt(row);
    endRemoveRows();

    emit filtersChanged();
}

void LdapFiltersModel::setFilter(
    int row,
    const QString& name,
    const QString& base,
    const QString& filter)
{
    if (!d->isRowValid(row))
        return;

    d->filters[row] = LdapFilter{
        .name = name,
        .base = base,
        .filter = filter
    };

    emit dataChanged(this->index(row, 0), this->index(row, 0));
    emit filtersChanged();
}

QList<LdapFilter> LdapFiltersModel::filters() const
{
    return d->filters;
}

void LdapFiltersModel::setFilters(const QList<LdapFilter>& value)
{
    const bool emitChanges = value != d->filters;

    const int diff = value.size() - d->filters.size();

    if (diff > 0)
    {
        beginInsertRows({}, d->filters.size(), d->filters.size() + diff - 1);
        d->filters = value;
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, d->filters.size() + diff, d->filters.size() - 1);
        d->filters = value;
        endRemoveRows();
    }
    else
    {
        d->filters = value;
    }

    if (d->filters.size() > 0)
    {
        emit dataChanged(
            this->index(0, 0),
            this->index(d->filters.size() - 1, 0));
    }

    if (emitChanges)
        emit filtersChanged();
}

void LdapFiltersModel::registerQmlType()
{
    qmlRegisterType<LdapFiltersModel>("nx.vms.client.desktop", 1, 0, "LdapFiltersModel");
}

} // namespace nx::vms::client::desktop
