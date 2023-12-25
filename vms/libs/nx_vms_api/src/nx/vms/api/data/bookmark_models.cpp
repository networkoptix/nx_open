// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_models.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

namespace {

std::optional<int> findIdSeparator(const QString& id, char separator)
{
    const auto p = id.indexOf(separator);
    if (p <= 0)
        return std::nullopt;

    return p;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkIdV1, (json), BookmarkIdV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkIdV3, (json), BookmarkIdV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkFilterV1, (json), BookmarkFilterV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkFilterV3, (json), BookmarkFilterV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkV1, (json), BookmarkV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkWithRuleV1, (json), BookmarkWithRuleV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkV3, (json), BookmarkV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkWithRuleV3, (json), BookmarkWithRuleV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkTagFilter, (json), BookmarkTagFilter_Fields)

void BookmarkIdV3::setIds(const QnUuid& bookmarkId, const QnUuid& serverId)
{
    id = bookmarkId.toSimpleString() + '_' + serverId.toSimpleString();
}

QnUuid BookmarkIdV3::bookmarkId() const
{
    if (id.isEmpty())
        return QnUuid();

    const auto p = findIdSeparator(id, '_');
    if (!p)
        return QnUuid::fromStringSafe(id);

    return QnUuid::fromStringSafe(id.mid(0, *p));
}

QnUuid BookmarkIdV3::serverId() const
{
    if (id.isEmpty())
        return QnUuid();

    const auto from = findIdSeparator(id, '_');
    if (!from)
        return QnUuid();

    const auto to = findIdSeparator(id, '.');
    return QnUuid::fromStringSafe(id.mid(*from + 1, to ? *to : -1));
}

} // namespace nx::vms::api
