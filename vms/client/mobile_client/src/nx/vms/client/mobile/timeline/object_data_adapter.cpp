// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_data_adapter.h"

#include <optional>

#include <QtCore/QPersistentModelIndex>
#include <QtQml/QtQml>

#include <core/resource/resource_fwd.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/qml/qml_ownership.h>

#include "analytics_data.h"
#include "bookmark_data.h"

namespace nx::vms::client::mobile {
namespace {

std::optional<common::CameraBookmark> getBookmark(const QModelIndex& index)
{
    if (const auto bookmark = index.data(core::CameraBookmarkRole); bookmark.isValid())
        return bookmark.value<common::CameraBookmark>();

    return std::nullopt;
}

std::optional<analytics::db::ObjectTrack> getTrack(const QModelIndex& index)
{
    if (const auto track = index.data(core::ObjectTrackRole); track.isValid())
        return track.value<analytics::db::ObjectTrack>();

    return std::nullopt;
}

bool inRange(const QModelIndex& index, const auto& topLeft, const auto& bottomRight)
{
    const int column = index.column();
    const int row = index.row();

    return qBetween(topLeft.column(), column, bottomRight.column() + 1)
        && qBetween(topLeft.row(), row, bottomRight.row() + 1);
}

void updateOnChange(
    const QPersistentModelIndex& index,
    QPointer<timeline::AbstractObjectData> objectData,
    std::function<void(const QModelIndex&)> updateFunction)
{
    if (!NX_ASSERT(objectData))
        return;

    QObject::connect(index.model(), &QAbstractItemModel::dataChanged, objectData,
        [index, updateFunction](const auto& topLeft, const auto& bottomRight, const auto& /*roles*/)
        {
            if (!index.isValid() || !inRange(index, topLeft, bottomRight))
                return;

            updateFunction(index);
        });
};

} // namespace

timeline::AbstractObjectData* ObjectDataAdapter::create(const QModelIndex& index)
{
    const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();

    if (!NX_ASSERT(resource))
        return nullptr;

    if (const auto bookmark = getBookmark(index))
    {
        QPointer result = new timeline::BookmarkData(*bookmark, resource); //< Qml ownership.

        updateOnChange(index, result,
            [result](const QModelIndex& index)
            {
                result->update(getBookmark(index).value_or(common::CameraBookmark{}));
            });

        return result;
    }
    else if (const auto track = getTrack(index))
    {
        QPointer result = new timeline::AnalyticsData(*track, resource); //< Qml ownership.

        updateOnChange(index, result,
            [result](const QModelIndex& index)
            {
                result->update(getTrack(index).value_or(nx::analytics::db::ObjectTrack{}));
            });

        return result;
    }

    return nullptr;
}

void ObjectDataAdapter::registerQmlType()
{
    qmlRegisterSingletonType<ObjectDataAdapter>("nx.vms.client.mobile", 1, 0, "ObjectDataAdapter",
        [](QQmlEngine*, QJSEngine*) { return new ObjectDataAdapter(); });
}

} // namespace nx::vms::client::mobile
