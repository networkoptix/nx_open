// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_filter.h"

#include <core/resource/camera_bookmark.h>

namespace nx::vms::common {

BookmarkTextFilter::BookmarkTextFilter(const QString& text)
{
    m_words = text.split(QRegularExpression("[\\s]"), Qt::SkipEmptyParts);
};

bool BookmarkTextFilter::operator()(const CameraBookmark& bookmark) const
{
    return match(bookmark);
}

bool BookmarkTextFilter::match(const CameraBookmark& bookmark) const
{
    for (const auto& word: m_words)
    {
        bool tagFound = false;

        for (const QString &tag: bookmark.tags)
        {
            if (tag.startsWith(word, Qt::CaseInsensitive))
            {
                tagFound = true;
                break;
            }
        }

        if (tagFound)
            continue;

        if (!bookmark.name.isEmpty() && bookmark.name.startsWith(word, Qt::CaseInsensitive))
            continue;

        if (bookmark.name.contains(word, Qt::CaseInsensitive)
            || bookmark.description.contains(word, Qt::CaseInsensitive))
        {
            continue;
        }

        return false;
    }

    return true;
}

} //namespace nx::vms::common
