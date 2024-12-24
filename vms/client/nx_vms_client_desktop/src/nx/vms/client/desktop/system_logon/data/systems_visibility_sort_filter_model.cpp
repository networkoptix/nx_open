// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_visibility_sort_filter_model.h"

#include <QtCore/QCollator>

#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <ui/models/systems_model.h>

using namespace nx::vms::client::core::welcome_screen;

namespace nx::vms::client::desktop {

namespace {

bool isLocalHost(const QModelIndex& index)
{
    // TODO: #dfisenko Provide this information using special role.
    return index.data(QnSystemsModel::SearchRoleId).toString().contains("localhost");
}

} // namespace

struct SystemsVisibilitySortFilterModel::Private
{
    nx::utils::ScopedConnections sourceModelConnections;
    QCollator collator;

    VisibilityScopeGetter visibilityScopeFilterGetter =
        []()
        {
            NX_ASSERT(false, "visibilityScopeGetter is not initialized yet");
            return Filter::AllSystemsTileScopeFilter;
        };
    VisibilityScopeSetter visibilityScopeFilterSetter =
        [](Filter /*filter*/)
        {
            NX_ASSERT(false, "visibilityScopeSetter is not initialized yet");
        };
    ForgottenCheckFunction systemIsForgotten =
        [](const QString& /*systemId*/)
        {
            NX_ASSERT(false, "systemIsForgotten is not initialized yet");
            return false;
        };
};

SystemsVisibilitySortFilterModel::SystemsVisibilitySortFilterModel(QObject* parent):
    base_type(parent),
    d(new Private)
{
    d->collator.setCaseSensitivity(Qt::CaseInsensitive);
    d->collator.setNumericMode(true);
    setDynamicSortFilter(true);
}

SystemsVisibilitySortFilterModel::~SystemsVisibilitySortFilterModel()
{
}

void SystemsVisibilitySortFilterModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    d->sourceModelConnections = {};

    base_type::setSourceModel(sourceModel);

    if (!sourceModel)
        return;

    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterRole(QnSystemsModel::SearchRoleId);

    d->sourceModelConnections <<
        QObject::connect(sourceModel, &QAbstractItemModel::dataChanged,
            [this](
                const QModelIndex& /*topLeft*/,
                const QModelIndex& /*bottomRight*/,
                const QVector<int>& roles)
            {
                static const QSet<int> kSortRoles = {
                    QnSystemsModel::IsFactorySystemRoleId,
                    QnSystemsModel::VisibilityScopeRoleId,
                    QnSystemsModel::IsCloudSystemRoleId,
                    QnSystemsModel::IsOnlineRoleId,
                    QnSystemsModel::SearchRoleId,
                    QnSystemsModel::SystemNameRoleId,
                    QnSystemsModel::IsCompatibleToDesktopClient
                };

                if (kSortRoles.contains(QSet<int>(roles.begin(), roles.begin())))
                    sort(0);
            });
    sort(0);

    d->sourceModelConnections << connect(sourceModel, &QnSystemsModel::rowsRemoved,
        this, &SystemsVisibilitySortFilterModel::totalSystemsCountChanged);

    d->sourceModelConnections << connect(sourceModel, &QnSystemsModel::rowsInserted,
        this, &SystemsVisibilitySortFilterModel::totalSystemsCountChanged);
}

void SystemsVisibilitySortFilterModel::setVisibilityScopeFilterCallbacks(
    SystemsVisibilitySortFilterModel::VisibilityScopeGetter getter,
    SystemsVisibilitySortFilterModel::VisibilityScopeSetter setter)
{
    d->visibilityScopeFilterGetter = getter;
    d->visibilityScopeFilterSetter = setter;
}

void SystemsVisibilitySortFilterModel::setForgottenCheckCallback(
    SystemsVisibilitySortFilterModel::ForgottenCheckFunction callback)
{
    d->systemIsForgotten = callback;
}

void SystemsVisibilitySortFilterModel::forgottenSystemAdded(const QString& systemId)
{
    if (!sourceModel())
        return;

    for (int row = 0; row < sourceModel()->rowCount(); ++row)
    {
        const auto index = sourceModel()->index(row, 0);
        if (systemId == sourceModel()->data(index, QnSystemsModel::SystemIdRoleId))
        {
            invalidateFilter();
            emit totalSystemsCountChanged();
            return;
        }
    }
}

void SystemsVisibilitySortFilterModel::forgottenSystemRemoved(const QString& systemId)
{
    if (!sourceModel())
        return;

    for (int row = 0; row < sourceModel()->rowCount(); ++row)
    {
        const auto index = sourceModel()->index(row, 0);
        if (systemId == sourceModel()->data(index, QnSystemsModel::SystemIdRoleId))
        {
            invalidateFilter();
            emit totalSystemsCountChanged();
            return;
        }
    }
}

SystemsVisibilitySortFilterModel::Filter SystemsVisibilitySortFilterModel::visibilityFilter() const
{
    return d->visibilityScopeFilterGetter();
}

void SystemsVisibilitySortFilterModel::setVisibilityFilter(
    SystemsVisibilitySortFilterModel::Filter filter)
{
    if (visibilityFilter() == filter)
        return;

    d->visibilityScopeFilterSetter(filter);

    emit visibilityFilterChanged();
    invalidateFilter();
}

int SystemsVisibilitySortFilterModel::totalSystemsCount() const
{
    return sourceModel()->rowCount() - forgottenSystemsCount(0, sourceModel()->rowCount() - 1);
}

bool SystemsVisibilitySortFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& /*sourceParent*/) const
{
    if (!sourceModel())
        return true;

    const auto sourceIndex = sourceModel()->index(sourceRow, 0);

    if (!sourceIndex.isValid())
        return true;

    if (d->systemIsForgotten(sourceIndex.data(QnSystemsModel::SystemIdRoleId).toString()))
        return false;

    if (filterRegularExpression().pattern().isEmpty())
        return !isHidden(sourceIndex);

    const auto fieldData = sourceModel()->index(sourceRow, 0).data(filterRole()).toString();
    return filterRegularExpression().match(fieldData).hasMatch();
}

bool SystemsVisibilitySortFilterModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto leftIsFactorySystem =
        sourceLeft.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactorySystem =
        sourceRight.data(QnSystemsModel::IsFactorySystemRoleId).toBool();

    const auto leftVisibilityScope =
        sourceLeft.data(QnSystemsModel::VisibilityScopeRoleId).value<TileVisibilityScope>();
    const auto rightVisibilityScope =
        sourceRight.data(QnSystemsModel::VisibilityScopeRoleId).value<TileVisibilityScope>();

    if (leftIsFactorySystem != rightIsFactorySystem)
    {
        if (leftVisibilityScope == rightVisibilityScope)
            return leftIsFactorySystem;

        return leftIsFactorySystem
            ? leftVisibilityScope != TileVisibilityScope::HiddenTileVisibilityScope
            : rightVisibilityScope == TileVisibilityScope::HiddenTileVisibilityScope;
    }

    if (leftVisibilityScope != rightVisibilityScope)
        return leftVisibilityScope > rightVisibilityScope;

    const bool leftIsConnectable = sourceLeft.data(QnSystemsModel::IsOnlineRoleId).toBool() &&
        sourceLeft.data(QnSystemsModel::IsCompatibleToDesktopClient).toBool();
    const bool rightIsConnectable = sourceRight.data(QnSystemsModel::IsOnlineRoleId).toBool() &&
        sourceRight.data(QnSystemsModel::IsCompatibleToDesktopClient).toBool();

    if (leftIsConnectable != rightIsConnectable)
        return leftIsConnectable;

    const bool leftIsCloud = sourceLeft.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
    const bool rightIsCloud = sourceRight.data(QnSystemsModel::IsCloudSystemRoleId).toBool();

    if (leftIsCloud != rightIsCloud)
        return leftIsCloud;

    const bool leftIsLocalhost = isLocalHost(sourceLeft);
    const bool rightIsLocalhost = isLocalHost(sourceRight);

    if (leftIsLocalhost != rightIsLocalhost)
        return leftIsLocalhost;

    const int namesOrder = d->collator.compare(
        sourceLeft.data(QnSystemsModel::SystemNameRoleId).toString(),
        sourceRight.data(QnSystemsModel::SystemNameRoleId).toString());

    if (namesOrder != 0)
        return namesOrder < 0;

    return sourceLeft.data(QnSystemsModel::SystemIdRoleId).toString()
        < sourceRight.data(QnSystemsModel::SystemIdRoleId).toString();
}

bool SystemsVisibilitySortFilterModel::isHidden(const QModelIndex& sourceIndex) const
{
    const auto visibilityScope =
        sourceIndex.data(QnSystemsModel::VisibilityScopeRoleId).value<TileVisibilityScope>();

    switch (visibilityFilter())
    {
        // When filter is set to 'Favorites', hide all which are not favorite.
        case TileScopeFilter::FavoritesTileScopeFilter:
            return visibilityScope != TileVisibilityScope::FavoriteTileVisibilityScope;

        // When filter is set to 'Hidden', hide all which are not hidden.
        case TileScopeFilter::HiddenTileScopeFilter:
            return visibilityScope != TileVisibilityScope::HiddenTileVisibilityScope;

        // Show all except hidden.
        default:
            return visibilityScope == TileVisibilityScope::HiddenTileVisibilityScope;
    }
}

int SystemsVisibilitySortFilterModel::forgottenSystemsCount(int firstRow, int lastRow) const
{
    int result = 0;

    if (!sourceModel())
        return result;

    for (int row = firstRow; row <= lastRow; ++row)
    {
        auto sourceIndex = sourceModel()->index(row, 0);
        if (d->systemIsForgotten(sourceIndex.data(QnSystemsModel::SystemIdRoleId).toString()))
        {
            ++result;
        }
    }

    return result;
}

} // namespace nx::vms::client::desktop
