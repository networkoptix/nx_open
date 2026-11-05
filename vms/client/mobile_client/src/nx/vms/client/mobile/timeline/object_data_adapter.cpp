// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_data_adapter.h"

#include <memory>
#include <optional>

#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/client_core_globals.h>

#include "analytics_data_helper.h"
#include "bookmark_data_helper.h"

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

/** Whether a model data change can affect the object data. Empty `roles` means all roles. */
bool affectsObjectData(const QList<int>& roles)
{
    return roles.isEmpty() || roles.contains(core::CameraBookmarkRole)
        || roles.contains(core::ObjectTrackRole);
}

bool inRange(const QModelIndex& index, const auto& topLeft, const auto& bottomRight)
{
    const int column = index.column();
    const int row = index.row();

    return qBetween(topLeft.column(), column, bottomRight.column() + 1)
        && qBetween(topLeft.row(), row, bottomRight.row() + 1);
}

} // namespace

EventSearchObjectData::EventSearchObjectData(const QModelIndex& index, QObject* parent):
    QObject(parent),
    m_index(index),
    m_resource(index.data(core::ResourceRole).value<QnResourcePtr>())
{
    update();

    if (!m_index.isValid())
        return;

    connect(m_index.model(),
        &QAbstractItemModel::dataChanged,
        this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
        {
            // Search models often change unrelated roles (e.g. timestamp text, visibility,
            // preview state), and each `dataChanged()` makes QML re-set the details item data.
            if (!m_index.isValid() || !affectsObjectData(roles)
                || !inRange(m_index, topLeft, bottomRight))
            {
                return;
            }

            update();
            emit dataChanged();
        });
}

const timeline::ObjectData& EventSearchObjectData::data() const
{
    return m_data;
}

QnResourcePtr EventSearchObjectData::resource() const
{
    return m_resource;
}

bool EventSearchObjectData::isValid() const
{
    return m_valid;
}

void EventSearchObjectData::update()
{
    if (!m_resource)
        return;

    if (const auto bookmark = getBookmark(m_index))
    {
        m_data = timeline::BookmarkDataHelper::makeObjectData(*bookmark, m_resource);
        m_valid = true;
    }
    else if (const auto track = getTrack(m_index))
    {
        m_data = timeline::AnalyticsDataHelper::makeObjectData(*track, m_resource);
        m_valid = true;
    }
}

QnResource* EventSearchObjectData::rawResource() const
{
    return m_resource.get();
}

EventSearchObjectData* ObjectDataAdapter::create(const QModelIndex& index)
{
    auto result = std::make_unique<EventSearchObjectData>(index);
    if (!NX_ASSERT(result->resource()) || !result->isValid())
        return nullptr;

    return result.release(); //< QML ownership.
}

void ObjectDataAdapter::registerQmlType()
{
    qmlRegisterSingletonType<ObjectDataAdapter>("nx.vms.client.mobile", 1, 0, "ObjectDataAdapter",
        [](QQmlEngine*, QJSEngine*) { return new ObjectDataAdapter(); });
}

} // namespace nx::vms::client::mobile
