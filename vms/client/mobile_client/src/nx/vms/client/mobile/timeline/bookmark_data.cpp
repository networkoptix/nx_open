// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_data.h"

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/qml/qml_ownership.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

BookmarkData::BookmarkData(
    const common::CameraBookmark& bookmark,
    const QnResourcePtr& resource)
    :
    m_resource(resource)
{
    update(bookmark);
}

nx::Uuid BookmarkData::id() const
{
    return m_bookmark.guid;
}

qint64 BookmarkData::startTimeMs() const
{
    return m_bookmark.startTimeMs.count();
}

qint64 BookmarkData::durationMs() const
{
    return m_bookmark.durationMs.count();
}

QString BookmarkData::title() const
{
    return m_bookmark.name.isEmpty() ? tr("Bookmark") : m_bookmark.name;
}

QString BookmarkData::description() const
{
    return m_bookmark.description;
}

QString BookmarkData::imagePath() const
{
    return m_imagePath;
}

QVariant BookmarkData::tags() const
{
    return QStringList{m_bookmark.tags.cbegin(), m_bookmark.tags.cend()};
}

QVariant BookmarkData::attributes() const
{
    return QVariantList{};
}

QnResourcePtr BookmarkData::resource() const
{
    return m_resource;
}

common::CameraBookmark BookmarkData::convertToBookmark() const
{
    return m_bookmark;
}

void BookmarkData::update(common::CameraBookmark bookmark)
{
    m_bookmark = std::move(bookmark);
    m_imagePath =
        makeImageRequest(m_resource, m_bookmark.startTimeMs.count(), kHighImageResolution);
    emit changed();
}

MultiObjectData BookmarkData::merge(
    std::span<const common::CameraBookmark> bookmarks,
    const QnTimePeriod& period,
    std::chrono::milliseconds minimumStackDuration,
    int limit,
    const QnResourcePtr& resource)
{
    QnTimePeriodList chunks;
    if ((int) bookmarks.size() > limit)
    {
        // If there are more than `limit` bookmarks, make one chunk covering the entire bucket.
        chunks.push_back(period);
    }
    else
    {
        chunks.reserve(bookmarks.size());

        for (auto it = bookmarks.rbegin(); it != bookmarks.rend(); ++it)
            chunks.push_back(QnTimePeriod(it->startTimeMs, it->durationMs).intersected(period));
    }

    const int count = (int) bookmarks.size();
    if (count == 1)
    {
        const auto& bookmark = bookmarks.front();

        const auto perObjectData = std::make_shared<BookmarkData>(bookmark, resource);

        return MultiObjectData{
            .caption = perObjectData->title(),
            .description = bookmark.description,
            .iconPaths = {"image://skin/20x20/Outline/bookmark.svg"},
            .imagePaths = {makeImageRequest(resource, bookmark.startTimeMs.count(),
                kLowImageResolution)},
            .positionMs = bookmark.startTimeMs.count(),
            .durationMs = bookmark.durationMs.count(),
            .count = 1,
            .objectChunks = chunks,
            .perObjectData = {perObjectData}};
    }
    else if (count > 1)
    {
        const auto title = count <= limit
            ? tr("Bookmarks (%1)").arg(count)
            : tr("Bookmarks (>%1)").arg(limit);

        const auto firstPosition = count <= limit
            ? bookmarks.back().startTimeMs
            : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

        const auto duration = bookmarks.front().startTimeMs - firstPosition;

        QList<std::shared_ptr<AbstractObjectData>> perObjectData;
        if (duration < minimumStackDuration)
        {
            for (const auto& bookmark: nx::utils::reverseRange(bookmarks))
            {
                perObjectData.push_back(std::make_shared<BookmarkData>(bookmark, resource));
            }
        }

        QStringList imagePaths;
        for (const auto& bookmark: bookmarks)
        {
            imagePaths.push_back(makeImageRequest(resource, bookmark.startTimeMs.count(),
                kLowImageResolution));

            if (imagePaths.size() >= kMaxPreviewImageCount)
                break;
        }

        return MultiObjectData{
            .caption = title,
            .iconPaths = {"image://skin/20x20/Outline/bookmark.svg"},
            .imagePaths = imagePaths,
            .positionMs = firstPosition.count(),
            .durationMs = duration.count(),
            .count = count,
            .objectChunks = chunks,
            .perObjectData = perObjectData};
    }
    else if (!chunks.empty())
    {
        return MultiObjectData{.objectChunks = chunks};
    }

    return {};
}

} // namespace timeline
} // namespace nx::vms::client::mobile
