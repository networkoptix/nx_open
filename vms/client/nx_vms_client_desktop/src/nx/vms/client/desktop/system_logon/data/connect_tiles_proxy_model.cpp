// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_tiles_proxy_model.h"

#include "systems_visibility_sort_filter_model.h"
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <ui/models/systems_model.h>
#include <client_core/client_core_settings.h>
#include <nx/utils/scoped_connections.h>

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

using PersistentIndexPair = QPair<
    QPersistentModelIndex, //< Proxy index.
    QPersistentModelIndex>; //< Source index.

struct ConnectTilesProxyModel::Private
{
    nx::utils::ScopedConnections sourceModelConnections;
    SystemsVisibilitySortFilterModel* visibilityModel;

    QVector<PersistentIndexPair> changingIndices;

    bool loggedToCloud = false;
    bool cloudTileEnabled = false;
    bool cloudTileIsHidden = false;

    bool connectTileEnabled = false;

    CloudTileVisibilityScopeGetter cloudTileScopeGetter =
        []()
        {
            NX_ASSERT(false, "cloudTileScopeGetter is not initialized yet");
            return TileVisibilityScope::DefaultTileVisibilityScope;
        };
    CloudTileVisibilityScopeSetter cloudTileScopeSetter =
        [](TileVisibilityScope)
        {
            NX_ASSERT(false, "cloudTileScopeSetter is not initialized yet");
        };
};

ConnectTilesProxyModel::ConnectTilesProxyModel(QObject* parent):
    base_type(parent),
    d(new Private)
{
}

ConnectTilesProxyModel::~ConnectTilesProxyModel()
{
}

void ConnectTilesProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    d->sourceModelConnections = {};

    auto prevSourceModel = base_type::sourceModel();
    base_type::setSourceModel(sourceModel);
    if (prevSourceModel)
        prevSourceModel->deleteLater();

    if (!sourceModel)
        return;

    try
    {
        d->visibilityModel = dynamic_cast<SystemsVisibilitySortFilterModel*>(sourceModel);
    }
    catch (std::bad_cast)
    {
        NX_CRITICAL(false, "No models except SystemsVisibilitySortFilterModel are supported.");
    }

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::dataChanged, this,
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight,
                const QVector<int>& roles)
            {
                emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
            });

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginInsertRows(
                    mapFromSource(parent),
                    mapRowFromSource(first),
                    mapRowFromSource(last));
            });
    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsInserted, this,
            [this]()
            {
                endInsertRows();
                emit systemsCountChanged();
                emit totalSystemsCountChanged();
            });

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex& parent, int first, int last)
            {
                beginRemoveRows(
                    mapFromSource(parent),
                    mapRowFromSource(first),
                    mapRowFromSource(last));
            });
    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsRemoved, this,
            [this]()
            {
                endRemoveRows();
                emit systemsCountChanged();
                emit totalSystemsCountChanged();
            });

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsAboutToBeMoved, this,
            [this](const QModelIndex& parent, int start, int end,
                const QModelIndex& destination, int row)
            {
                beginMoveRows(
                    mapFromSource(parent), mapRowFromSource(start), mapRowFromSource(end),
                    mapFromSource(destination), mapRowFromSource(row));
            });
    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::rowsMoved, this,
            [this]()
            {
                endMoveRows();
            });

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::layoutAboutToBeChanged, this,
            [this](
                const QList<QPersistentModelIndex>& parents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                // sourceModel is SystemsVisibilitySortFilterModel which is flat list model.
                NX_ASSERT(parents.size() == 1 && !parents.first().isValid());

                emit layoutAboutToBeChanged({QModelIndex()}, hint);

                NX_ASSERT(d->changingIndices.empty());
                d->changingIndices.clear(); //< Just in case.

                const auto persistentIndices = persistentIndexList();
                d->changingIndices.reserve(persistentIndices.size());

                for (const QModelIndex& index: persistentIndices)
                    d->changingIndices << PersistentIndexPair(index, mapToSource(index));
            });
    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::layoutChanged, this,
            [this](
                const QList<QPersistentModelIndex>& parents,
                QAbstractItemModel::LayoutChangeHint hint)
            {
                // sourceModel is SystemsVisibilitySortFilterModel which is flat list model.
                NX_ASSERT(parents.size() == 1 && !parents.first().isValid());

                for (const auto& changed: d->changingIndices)
                    changePersistentIndex(changed.first, mapFromSource(changed.second));

                d->changingIndices.clear();

                emit layoutChanged({QModelIndex()}, hint);
            });

    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::modelAboutToBeReset, this,
            [this]()
            {
                beginResetModel();
            });
    d->sourceModelConnections <<
        connect(sourceModel, &QAbstractListModel::modelReset, this,
            [this]()
            {
                endResetModel();
                emit systemsCountChanged();
                emit totalSystemsCountChanged();
            });

    d->sourceModelConnections <<
        connect(d->visibilityModel, &SystemsVisibilitySortFilterModel::visibilityFilterChanged,
            this, &ConnectTilesProxyModel::visibilityFilterChanged);

    connect(d->visibilityModel, &SystemsVisibilitySortFilterModel::totalSystemsCountChanged,
        this, &ConnectTilesProxyModel::totalSystemsCountChanged);

    if (!d->loggedToCloud && !isCloudTileVisibilityScopeHidden())
        setCloudTileEnabled(true);
    if (d->visibilityModel->visibilityFilter() == welcome_screen::AllSystemsTileScopeFilter)
        setConnectTileEnabled(true);
}

void ConnectTilesProxyModel::setCloudVisibilityScopeCallbacks(
    ConnectTilesProxyModel::CloudTileVisibilityScopeGetter getter,
    ConnectTilesProxyModel::CloudTileVisibilityScopeSetter setter)
{
    d->cloudTileScopeGetter = getter;
    d->cloudTileScopeSetter = setter;
}

QVariant ConnectTilesProxyModel::data(const QModelIndex& index, int role) const
{
    if (!sourceModel())
        return QVariant();

    switch (role)
    {
        case TileTypeRoleId:
        {
            if (cloudTileIsVisible() && isFirstIndex(index))
                return CloudButtonTileType;

            if (d->connectTileEnabled && isLastIndex(index))
                return ConnectButtonTileType;

            return GeneralTileType;
        }
        case QnSystemsModel::VisibilityScopeRoleId:
        {
            if (cloudTileIsVisible() && isFirstIndex(index))
            {
                return isCloudTileVisibilityScopeHidden()
                    ? welcome_screen::HiddenTileVisibilityScope
                    : welcome_screen::DefaultTileVisibilityScope;
            }
        }
        default:
        {
            if ((cloudTileIsVisible() && isFirstIndex(index))
                || (d->connectTileEnabled && isLastIndex(index)))
            {
                return QVariant();
            }
        }
    }

    return sourceModel()->data(mapToSource(index), role);
}

bool ConnectTilesProxyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!sourceModel())
        return false;

    if (role == QnSystemsModel::VisibilityScopeRoleId)
    {
        if (cloudTileIsVisible() && isFirstIndex(index))
        {
            d->cloudTileScopeSetter(
                static_cast<welcome_screen::TileVisibilityScope>(value.toInt()));

            setCloudTileEnabled(!d->loggedToCloud && cloudTileIsAcceptedByFilter());
            return true;
        }
    }

    return sourceModel()->setData(mapToSource(index), value, role);
}

QVariant ConnectTilesProxyModel::visibilityFilter() const
{
    if (!d->visibilityModel)
        return SystemsVisibilitySortFilterModel::Filter::AllSystemsTileScopeFilter;

    return d->visibilityModel->visibilityFilter();
}

void ConnectTilesProxyModel::setVisibilityFilter(QVariant filter)
{
    if (!d->visibilityModel)
        return;

    if (visibilityFilter() == filter)
        return;

    const welcome_screen::TileScopeFilter filterValue =
        static_cast<SystemsVisibilitySortFilterModel::Filter>(filter.toInt());

    if (filterValue == welcome_screen::AllSystemsTileScopeFilter)
    {
        setConnectTileEnabled(true);
        if (!d->loggedToCloud)
            setCloudTileEnabled(!isCloudTileVisibilityScopeHidden());
    }
    else if (filterValue == welcome_screen::FavoritesTileScopeFilter)
    {
        setConnectTileEnabled(false);
        setCloudTileEnabled(false);
    }
    else if (filterValue == welcome_screen::HiddenTileScopeFilter)
    {
        setConnectTileEnabled(false);
        if (!d->loggedToCloud)
            setCloudTileEnabled(isCloudTileVisibilityScopeHidden());
    }

    d->visibilityModel->setVisibilityFilter(filterValue);
}

int ConnectTilesProxyModel::systemsCount() const
{
    return rowCount();
}

int ConnectTilesProxyModel::totalSystemsCount() const
{
    if (!d->visibilityModel)
        return 0;

    return d->visibilityModel->totalSystemsCount()
        /* "Login to cloud" tile if not logged in to the Cloud.
         * If the tile is hidden, it's not counted in total systems number,
         * to not provoke WS transition to complex visibility mode.
         */
        + (d->loggedToCloud || isCloudTileVisibilityScopeHidden() ? 0 : 1)
        + 1; //< "Connect to Server" tile.
}

QModelIndex ConnectTilesProxyModel::index(
    int row,
    int column,
    const QModelIndex& /*parent*/) const
{
    return createIndex(row, column, nullptr);
}

QModelIndex ConnectTilesProxyModel::parent(const QModelIndex& /*index*/) const
{
    return QModelIndex();
}

int ConnectTilesProxyModel::rowCount(const QModelIndex& /*parent*/) const
{
    if (!sourceModel())
        return 0;

    int result = sourceModel()->rowCount();
    if (d->cloudTileEnabled)
        ++result;
    if (d->connectTileEnabled)
        ++result;
    return result;
}

int ConnectTilesProxyModel::columnCount(const QModelIndex& parent) const
{
    if (!sourceModel())
        return 0;

    return sourceModel()->columnCount(parent);
}

QModelIndex ConnectTilesProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!sourceModel())
        return proxyIndex;

    const int row = proxyIndex.row();
    if (row < 0 || row >= rowCount())
        return QModelIndex();

    const bool isCloudTile = isFirstIndex(proxyIndex) && cloudTileIsVisible();
    const bool isConnectTile = isLastIndex(proxyIndex) && d->connectTileEnabled;

    if (isCloudTile || isConnectTile)
    {
        NX_ASSERT(false, "Invalid mapToSource call");
        return QModelIndex();
    }

    return sourceModel()->index(mapRowToSource(row), 0);
}

QModelIndex ConnectTilesProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceModel())
        return sourceIndex;

    const int row = sourceIndex.row();
    if (row < 0 || row >= sourceModel()->rowCount())
        return QModelIndex();

    return index(mapRowFromSource(row), 0);
}

QHash<int, QByteArray> ConnectTilesProxyModel::roleNames() const
{
    auto result = base_type::roleNames();
    result[TileTypeRoleId] = "tileType";
    return result;
}

bool ConnectTilesProxyModel::loggedToCloud() const
{
    return d->loggedToCloud;
}

void ConnectTilesProxyModel::setLoggedToCloud(bool isLogged)
{
    d->loggedToCloud = isLogged;

    setCloudTileEnabled(!d->loggedToCloud && cloudTileIsAcceptedByFilter());

    emit totalSystemsCountChanged();
}

void ConnectTilesProxyModel::setFilterWildcard(const QString& filterWildcard)
{
    if (d->visibilityModel)
        d->visibilityModel->setFilterWildcard(filterWildcard);

    if (!filterWildcard.isEmpty())
    {
        setConnectTileEnabled(false);
        setCloudTileEnabled(false);
    }
    else
    {
        setConnectTileEnabled(visibilityFilter() == welcome_screen::AllSystemsTileScopeFilter);
        setCloudTileEnabled(!d->loggedToCloud && cloudTileIsAcceptedByFilter());
    }
}

bool ConnectTilesProxyModel::cloudTileIsVisible() const
{
    return d->cloudTileEnabled && cloudTileIsAcceptedByFilter();
}

bool ConnectTilesProxyModel::cloudTileIsAcceptedByFilter() const
{
    return (visibilityFilter() == welcome_screen::AllSystemsTileScopeFilter
            && !isCloudTileVisibilityScopeHidden())
        || (visibilityFilter() == welcome_screen::HiddenTileScopeFilter
            && isCloudTileVisibilityScopeHidden());
}

bool ConnectTilesProxyModel::isCloudTileVisibilityScopeHidden() const
{
    return d->cloudTileScopeGetter() == welcome_screen::HiddenTileVisibilityScope;
}

void ConnectTilesProxyModel::setCloudTileEnabled(bool enabled)
{
    if (enabled == d->cloudTileEnabled)
        return;

    if (enabled)
    {
        ScopedInsertRows insertRows(this, 0, 0);
        d->cloudTileEnabled = enabled;
    }
    else
    {
        ScopedRemoveRows removeRows(this, 0, 0);
        d->cloudTileEnabled = enabled;
    }
}

void ConnectTilesProxyModel::setConnectTileEnabled(bool enabled)
{
    if (enabled == d->connectTileEnabled)
        return;

    if (enabled)
    {
        ScopedInsertRows insertRows(this, rowCount(), rowCount());
        d->connectTileEnabled = enabled;
    }
    else
    {
        ScopedRemoveRows removeRows(this, rowCount() - 1, rowCount() - 1);
        d->connectTileEnabled = enabled;
    }
}

bool ConnectTilesProxyModel::isFirstIndex(const QModelIndex& index) const
{
    return index.row() == 0;
}

bool ConnectTilesProxyModel::isLastIndex(const QModelIndex& index) const
{
    return index.row() == rowCount() - 1;
}

int ConnectTilesProxyModel::mapRowToSource(int row) const
{
    return cloudTileIsVisible() ? row - 1 : row;
}

int ConnectTilesProxyModel::mapRowFromSource(int row) const
{
    return cloudTileIsVisible() ? row + 1 : row;
}

} // namespace nx::vms::client::desktop
