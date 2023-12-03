// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_sort.h"

#include <algorithm>

#include <QtCore/QCoreApplication>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/algorithm/comparator.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/api/data/bookmark_models.h>

#include "bookmark_facade.h"

using namespace nx::vms::common;
using nx::vms::api::BookmarkSortField;

namespace {

template<typename Bookmark, typename GetterFunc>
std::function<bool(const Bookmark&, const Bookmark&)>
    createSortPredicate(bool ascending, GetterFunc getterFunc)
{
    return nx::utils::algorithm::Comparator(ascending, getterFunc, &BookmarkFacade<Bookmark>::id);
}

template<typename Bookmark>
std::function<bool(const Bookmark&, const Bookmark&)>
    createGenericBookmarkSortPredicate(
        BookmarkSortField sortField,
        bool ascending,
        QnResourcePool* resourcePool)
{
    using nx::utils::algorithm::Comparator;
    switch (sortField)
    {
        case BookmarkSortField::name:
            return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::name);

        case BookmarkSortField::description:
            return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::description);

        case BookmarkSortField::startTime:
            return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::startTime);

        case BookmarkSortField::duration:
            return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::durationMs);

        case BookmarkSortField::creationTime:
            return createSortPredicate<Bookmark>(
                ascending, &BookmarkFacade<Bookmark>::creationTimeMs);

        case BookmarkSortField::tags:
            return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::tags);

        case BookmarkSortField::creator:
            return createSortPredicate<Bookmark>(ascending,
                [resourcePool](const Bookmark& bookmark)
                {
                    return BookmarkFacade<Bookmark>::creatorName(bookmark, resourcePool);
                });

        case BookmarkSortField::cameraName:
            return createSortPredicate<Bookmark>(ascending,
                [resourcePool](const Bookmark& bookmark)
                {
                    return BookmarkFacade<Bookmark>::cameraName(bookmark, resourcePool);
                });
    }

    NX_ASSERT(false, "Invalid bookmark sort field: '%1'", sortField);
    return createSortPredicate<Bookmark>(ascending, &BookmarkFacade<Bookmark>::id);
}

} // namespace

std::function<bool(const QnCameraBookmark&, const QnCameraBookmark&)>
    createBookmarkSortPredicate(
        BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool)
{
    return createGenericBookmarkSortPredicate<QnCameraBookmark>(
        sortField, sortOrder == Qt::AscendingOrder, resourcePool);
}

namespace nx::vms::common {

std::function<bool(const nx::vms::api::Bookmark&, const nx::vms::api::Bookmark&)>
    createBookmarkSortPredicate(
    nx::vms::api::BookmarkSortField sortField,
    Qt::SortOrder sortOrder,
    QnResourcePool* resourcePool)
{
    return createGenericBookmarkSortPredicate<nx::vms::api::Bookmark>(
        sortField, sortOrder == Qt::AscendingOrder, resourcePool);
}

} // namespace nx::vms::api
